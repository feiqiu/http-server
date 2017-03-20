#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include "http_parser.h"

typedef struct http_kv {
	char key[128];
	char value[128];
} http_kv;

typedef struct http_head {
	int cnt;
	http_kv kv[64];
} http_head;

typedef http_head http_par;

typedef struct http_request_body {
	char filename[128];
	int file_len;
	int file_writed;
	int file_fd;
	char body[1024];
	int body_len;
} http_request_content;


typedef struct http_request {
	int fd;
	int complete;

	http_head head;
	http_par url_par;
	char method[64];
	char route[64];
	http_request_content content;
} http_request;

typedef struct http_response {
	int fd;
	http_head head;
	char *body;
	int body_len;
	int status;
} http_response;

typedef int (*http_req_cb)(http_request *req);

typedef struct http_route {
	char method[64];
	char route[64];
	http_req_cb cb;
} http_route;

typedef struct http_route_m {
	int route_cnt;
	http_route route[64];

} http_route_m;

extern http_response http_response_create(http_request *req);

extern int http_response_set_head(http_response *rep, char *key, char *value);

extern int http_response_set_body(http_response *rep, char *body, int len);

extern int http_response_set_status(http_response *rep, int status);

extern int http_response_send(http_response *rep);

extern int http_route_add(char *route, char *method, http_req_cb cb);

extern int server_init();

extern int server_uninit();
#endif
