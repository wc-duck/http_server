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

#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

#include <stdlib.h>

/**
 * Handle to initialized http-server.
 */
typedef struct http_server* http_server_t;

enum http_server_request_type
{
	HTTP_SERVER_REQUEST_GET,
	HTTP_SERVER_REQUEST_HEAD,
	HTTP_SERVER_REQUEST_POST,
	HTTP_SERVER_REQUEST_PUT,
	HTTP_SERVER_REQUEST_DELETE,

	// add "websocket data" and function to send websocket data and track it ...
};

/**
 * Allocator used together with functions that need to allocate dynamic memory.
 *
 * alloc function should work as malloc().
 *
 * @example
 *
 * void* my_alloc( size_t sz, my_allocator* self )
 * {
 *     return some_alloc_func( sz, self->my_userdata );
 * }
 *
 * void my_free( void* ptr, my_allocator* self )
 * {
 *     some_free_func( ptr, self->my_userdata );
 * }
 *
 * struct my_allocator
 * {
 *     http_server_allocator alloc;
 *     int my_userdata;
 * };
 *
 * my_alloc a = { { my_alloc, my_free }, 1337 };
 */
struct http_server_allocator
{
	void* (*alloc)( size_t sz, http_server_allocator* self );
	void  (*free) ( void* ptr, http_server_allocator* self );
};

/**
 * Struct representing a specific request made to the server.
 */
struct http_server_request
{
	http_server_request_type type; ///< type of request that was done.
	const char* path;              ///< path requested.
};

/**
 * Create a new http server.
 *
 * @param port to listen to connections on.
 * @param max_pending_connections maximum amount of waiting connections between polls.
 * @param alloc allocator to use when allocating memory for the server, if 0x0 malloc/free will be used.
 *
 * @return new http_server_t or 0x0 on error.
 */
http_server_t http_server_create( unsigned int port, unsigned int max_pending_connections, http_server_allocator* alloc );

/**
 * Destroy http_server_t created by http_server_create().
 */
void http_server_destroy( http_server_t server );

/**
 * Poll one outstanding request to handle from the server.
 *
 * @param server to poll.
 * @param alloc allocator used to alloc any new request, memory allocated need to stay valid until the returned request
 *              is finalized with either http_server_complete_chunked_request(), http_server_complete_request() or
 *              http_server_fail_request().
 *
 * @return new request or 0x0 if no more requests was present at this time.
 */
http_server_request* http_server_poll( http_server_t server, http_server_allocator* alloc );

/**
 * Send chunk in a chunked request, if a request is started with a call to this function it need to be finalized with
 * a call to http_server_complete_chunked_request().
 *
 * @see http://en.wikipedia.org/wiki/Chunked_transfer_encoding
 *
 * @param req request to send chunk for.
 * @param mimetype of data that is sent.
 * @param data pointer to data to send in chunk.
 * @param data_size size of buffer pointed to by data.
 */
void http_server_process_chunked_request( http_server_request* req, const char* mimetype, const void* data, size_t data_size );

/**
 * Complete a chunked request started by a call to http_server_process_chunked_request.
 *
 * @param req request to complete.
 */
void http_server_complete_chunked_request( http_server_request* req );

/**
 * Complete a single request with a single data block.
 *
 * @param req request to complete.
 * @param mimetype of data that is sent.
 * @param data pointer to data to send.
 * @param data_size size of buffer pointed to by data.
 */
void http_server_complete_request( http_server_request* req, const char* mimetype, const void* data, size_t data_size );

/**
 * Fail a request.
 *
 * @param req request to fail.
 * @param error_code http error code to respond with to the caller.
 */
void http_server_fail_request( http_server_request* req, int error_code ); // TODO: error string?

#endif // HTTP_SERVER_H_INCLUDED
