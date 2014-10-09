#include <http_server/http_server.h>

#include <stdio.h>
#include <string.h>

inline bool strend( const char* str, const char* end )
{
	size_t len = strlen( str );
	size_t end_len = strlen( end );
	if( len < end_len )
		return false;
	return strcmp( str + len - end_len, end ) == 0;
}

static const char* find_mime_type( const char* filename )
{
	if( strend( filename, ".html" ) ) return "text/html";
	if( strend( filename, ".htm" ) )  return "text/html";
	if( strend( filename, ".txt" ) )  return "text/plain";
	return "application/octet-stream";
}

int main( int /*argc*/, char** /*argv*/ )
{
	http_server_t server = http_server_create( 1337, 16, 0x0 );
	if( server == 0x0 )
		return 1;

	bool running = true;
	while( running )
	{
		http_server_request* req;
		while( ( req = http_server_poll( server, 0x0 ) ) != 0x0 )
		{
			switch( req->type )
			{
				case HTTP_SERVER_REQUEST_GET:
				{
					FILE* f = fopen( req->path + 1, "rb" );

					if( f == 0x0 )
					{
						http_server_fail_request( req, 404 );
						break;
					}

					const char* mimetype = find_mime_type( req->path );

					char read_buff[8192];
					size_t read = 0;
					do
					{
						read = fread( read_buff, 1, sizeof(read_buff), f );
						http_server_process_chunked_request( req, mimetype, read_buff, read );
					} while( read == sizeof(read_buff) );

					http_server_complete_chunked_request( req );
				}
				break;

				case HTTP_SERVER_REQUEST_HEAD:
				case HTTP_SERVER_REQUEST_POST:
				case HTTP_SERVER_REQUEST_PUT:
				case HTTP_SERVER_REQUEST_DELETE:
					http_server_fail_request( req, 405 ); // method not allowed.
					break;
				default:
					http_server_fail_request( req, 500 ); // internal server error.
					break;
			}
		}
	}

	http_server_destroy( server );
}
