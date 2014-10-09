#include <http_server/http_server.h>

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
					if(false /*not file exists*/)
					{
						http_server_fail_request( req, 404 ); // method not allowed.
					}
					else
					{
						http_server_complete_request( req, "application/json", "{ \"test\" : true }", 6 );
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
