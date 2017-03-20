#include <stdio.h>

#include "common.h"
#include "http_server.h"

static int cgi_msg(http_request *req) {
	http_response rep;
	char body[1024] = "{"
			"\"status\":\"ok\""
			"}";

	rep = http_response_create(req);
	http_response_set_status(&rep, HTTP_STATUS_OK);
	http_response_set_head(&rep, "Content-Type", "application/json");
	http_response_set_body(&rep, body, strlen(body));
	http_response_send(&rep);
	return 0;
}


int main(int argc, char ** argv){
	http_route_add("/api/msg", "get", cgi_msg);
	http_route_add("/api/file", "post", cgi_msg);
	server_init();
	
	return 0;
}
