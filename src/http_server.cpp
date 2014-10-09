/*
    Simple http-server written to be an drop-in code, focus is to be small
    and easy to use but maybe not efficient.
    It also tries to make as few decisions for the user as possible, decisions
    such as how to allocate memory and how to process requests ( direct in loop,
    thread? )

    version 1.0, October, 2014

	Copyright (C) 2014- Fredrik Kihlander

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	Fredrik Kihlander
*/

#include <http_server/http_server.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct http_server
{
	http_server_allocator* alloc;

	int serverfd;

	int maxfd;
	int newmax;

	fd_set listener_fds;
	fd_set master_fds;
	fd_set read_fds;

	bool start_poll;
	int  scan_pos;
};

struct http_server_request_impl
{
	http_server_request req;

	int sock;
	int first_chunk;
	http_server_allocator* alloc;
};

static void* http_server_alloc( size_t size, http_server_allocator* alloc )
{
	if( alloc == 0x0 )
		return malloc( size );
	return alloc->alloc( size, alloc );
}

static void http_server_free( void* ptr, http_server_allocator* alloc )
{
	if( alloc == 0x0 )
		free( ptr );
	else
		alloc->free( ptr, alloc );
}

static void http_server_send_fmt( int sockfd, const char* fmt, ... )
{
	char buffer[2048];

	va_list arg_ptr;
	va_start(arg_ptr, fmt);
	int sz = vsnprintf( buffer, sizeof( buffer ), fmt, arg_ptr );
	va_end(arg_ptr);

	if( send( sockfd, buffer, sz, 0 ) == -1 )
		printf("e1\n"); // handle error
}

http_server_t http_server_create( unsigned int port, unsigned int max_pending_connections, http_server_allocator* alloc )
{
	http_server* serv = (http_server*)http_server_alloc( sizeof( http_server ), alloc );
	if( serv == 0x0 )
		return 0x0;

	serv->alloc = alloc;

	// ... start server ...
	serv->serverfd = socket( AF_INET, SOCK_STREAM, 0 );
	if( serv->serverfd < 0 )
		return 0x0; // TODO: handle error and no mem-leaks!

	// ... return port to system directly at shutdown ...
	int optval = 1;
	setsockopt( serv->serverfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int) );

	struct sockaddr_in serveraddr;
	memset( &serveraddr, 0x0, sizeof(serveraddr) );
	serveraddr.sin_family      = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port        = htons( (unsigned short)port );

	if( bind( serv->serverfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr) ) < 0 )
	{
		http_server_destroy( serv );
		return 0x0;
	}

	if( listen( serv->serverfd, max_pending_connections ) < 0 )
	{
		http_server_destroy( serv );
		return 0x0;
	}

	serv->maxfd  = serv->serverfd;
	serv->newmax = serv->serverfd;
	serv->start_poll = true;
	FD_ZERO( &serv->master_fds );
	FD_ZERO( &serv->read_fds );
	FD_SET( serv->serverfd, &serv->master_fds );
	return serv;
}

void http_server_destroy( http_server_t server )
{
	// ... close ports etc ...
	http_server_free( server, server->alloc );
}

http_server_request* http_server_poll( http_server_t server, http_server_allocator* alloc )
{
	if( server->start_poll )
	{
		server->read_fds = server->master_fds;
		if( select( server->maxfd + 1, &server->read_fds, NULL, NULL, NULL ) == -1 )
			return 0x0;

		server->start_poll = false;
		server->scan_pos   = 0;
	}

	for( ; server->scan_pos <= server->maxfd ; ++server->scan_pos )
	{
		if( !FD_ISSET( server->scan_pos, &server->read_fds ) )
			continue;

		if( server->scan_pos == server->serverfd )
		{
			// ... new connection ...
			int connfd;
			struct sockaddr_in clientaddr;
			unsigned int addrlen = sizeof(clientaddr);
			if( ( connfd = accept( server->serverfd, (struct sockaddr *)&clientaddr, &addrlen ) ) == -1 )
				return 0x0;

			FD_SET( connfd, &server->master_fds );

			if( connfd > server->newmax )
				server->newmax = connfd;
		}
		else
		{
			// ... new data at socket ...
			char buf[1024];
			ssize_t nbytes;
			if( ( nbytes = recv( server->scan_pos, buf, sizeof(buf), 0) ) <= 0 )
			{
				if(nbytes == 0)
					printf("socket hung up\n");
			}

			// ... scan lines ...

			// ... haxxy ...

			http_server_request_type req_type;
			int verb_len = 0;
			     if( strncmp( buf, "GET",    3 ) == 0 ) { req_type = HTTP_SERVER_REQUEST_GET;    verb_len = 3; }
			else if( strncmp( buf, "PUT",    3 ) == 0 ) { req_type = HTTP_SERVER_REQUEST_PUT;    verb_len = 3; }
			else if( strncmp( buf, "HEAD",   4 ) == 0 ) { req_type = HTTP_SERVER_REQUEST_HEAD;   verb_len = 4; }
			else if( strncmp( buf, "POST",   4 ) == 0 ) { req_type = HTTP_SERVER_REQUEST_POST;   verb_len = 4; }
			else if( strncmp( buf, "DELETE", 6 ) == 0 ) { req_type = HTTP_SERVER_REQUEST_DELETE; verb_len = 6; }
			else
				printf("ERROR!!!\n");

			char* path_start = &buf[ verb_len + 1 ];

			// TODO: I guess this can "parse badly" and read out of bounds!
			int path_len = 0;
			while( !isspace( path_start[ path_len ] ) )
				++path_len;

			http_server_request_impl* req = (http_server_request_impl*)http_server_alloc( sizeof(http_server_request_impl) + path_len + 1, alloc );
			req->req.path = (char*)req + sizeof( http_server_request_impl );
			memcpy( (char*)req->req.path, path_start, path_len );
			((char*)req->req.path)[path_len] = '\0';
			req->req.type = req_type;
			req->sock = server->scan_pos;
			req->first_chunk = 1;
			req->alloc = alloc;

			return &req->req;
		}
	}

	if( server->scan_pos > server->maxfd )
	{
		server->maxfd = server->newmax;
		server->start_poll = true;
	}

	return 0x0;
}

static void http_server_free_request( http_server_request_impl* req )
{
	http_server_free( (char*)req, req->alloc );
}

void http_server_complete_request( http_server_request* _req, const char* mimetype, const void* data, size_t data_size )
{
	if( mimetype == 0x0 )
		mimetype = "text/html";

	http_server_request_impl* req = (http_server_request_impl*)_req;
	http_server_send_fmt( req->sock, "HTTP/1.1 200 OK\r\n"
									 "Content-Type: %s\r\n"
									 "Content-Length: %lu\r\n\r\n", mimetype, data_size );

	if( send( req->sock, data, data_size, 0 ) == -1 )
		printf("e2\n"); // handle error

	http_server_free_request( req );
}

void http_server_process_chunked_request( http_server_request* _req, const char* mimetype, const void* data, size_t data_size )
{
	if( mimetype == 0x0 )
		mimetype = "text/html";

	http_server_request_impl* req = (http_server_request_impl*)_req;

	if( req->first_chunk )
	{
		http_server_send_fmt( req->sock, "HTTP/1.1 200 OK\r\n"
									     "Content-Type: %s\r\n"
									     "Transfer-Encoding: chunked\r\n\r\n", mimetype );
		req->first_chunk = 0;
	}

	http_server_send_fmt( req->sock, "%lx\r\n", data_size );

	if( send( req->sock, data, data_size, 0 ) == -1 )
		printf("e1\n"); // handle error

	if( send( req->sock, "\r\n", 2, 0 ) == -1 )
		printf("e1\n"); // handle error
}

void http_server_complete_chunked_request( http_server_request* _req )
{
	http_server_request_impl* req = (http_server_request_impl*)_req;
	if( send( req->sock, "0\r\n\r\n", 5, 0 ) == -1 )
		printf("e1\n"); // handle error

	http_server_free_request( req );
}

static const char* http_server_error_code_to_str( int error_code )
{
	switch( error_code )
	{
		case 403: return "403 Forbidden";
		case 404: return "404 Not Found";
		default:
			return "500 Internal Server Error";
	}
}

static void http_server_send_default_error_page( int sockfd, const char* err_as_str )
{
	char err_page_buf[2048];
	int  err_page_sz = snprintf( err_page_buf, sizeof( err_page_buf ), "<http><head><title>%s</title></head><body><h1>%s</h1></body></http>", err_as_str, err_as_str );

	http_server_send_fmt( sockfd, "Content-Length: %d\r\n\r\n", err_page_sz );

	if( send( sockfd, err_page_buf, err_page_sz, 0 ) == -1 )
		printf("e2\n"); // handle error
}

void http_server_fail_request( http_server_request* _req, int error_code )
{
	http_server_request_impl* req = (http_server_request_impl*)_req;

	const char* err_as_str = http_server_error_code_to_str( error_code );
	http_server_send_fmt( req->sock, "HTTP/1.1 %s\r\n"
									 "Content-Type: text/html\r\n",
									 err_as_str );

	http_server_send_default_error_page( req->sock, err_as_str );
	http_server_free_request( req );
}
