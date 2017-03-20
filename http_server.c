#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"
#include "http_server.h"

#define SERVER_IP		("0.0.0.0")
#define SERVER_PORT		(8888)

static int g_server_exit = 0;

static http_route_m g_routes;

static const char *status_strings[] = {
#define XX(num, name, string) [HTTP_STATUS_##name]=#string,
		HTTP_STATUS_MAP(XX)
#undef XX
};

#ifndef ELEM_AT
# define ELEM_AT(a, i, v) ((unsigned int) (i) < ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static const char * http_status_str(enum http_status m) {
	return ELEM_AT(status_strings, m, "<unknown>");
}


extern http_response http_response_create(http_request *req){
	http_response rep;

	bzero(&rep, sizeof(rep));
	rep.fd = req->fd;
	return rep;
}

extern int http_response_set_head(http_response *rep, char *key, char *value){
	strcpy(rep->head.kv[rep->head.cnt].key, key);
	strcpy(rep->head.kv[rep->head.cnt].value, value);
	return 0;
}


extern int http_response_set_body(http_response *rep, char *body, int len){
	rep->body = (char *)malloc(len);
	memcpy(rep->body, body, len);
	rep->body_len = len;
}

extern int http_response_set_status(http_response *rep, int status){
	rep->status = status;
	return 0;
}

extern int http_response_send(http_response *rep){
	char head[1024] = { 0 };
	int i = 0;

	snprintf(head, 1024, "HTTP/1.1 %d %s\r\n"
			"Content-Length: %d\r\n"
			"Connection:close\r\n"
			"Server:qiuwei\r\n", rep->status,  http_status_str(rep->status), rep->body_len);

	for(i = 0; i < rep->head.cnt; i++){
		sprintf(head + strlen(head), "%s:%s\r\n", rep->head.kv[i].key, rep->head.kv[i].value);
	}
	sprintf(head + strlen(head), "\r\n");

	send(rep->fd, head, strlen(head), 0);
	send(rep->fd, rep->body, rep->body_len, 0);
	return 0;
}

extern int http_route_add(char *route, char *method, http_req_cb cb) {
	g_routes.route[g_routes.route_cnt].cb = cb;
	strcpy(g_routes.route[g_routes.route_cnt].route, route);
	strcpy(g_routes.route[g_routes.route_cnt].method, method);
	g_routes.route_cnt++;
}

static int http_req_handle(http_request *req){
	int i = 0;

	for (i = 0; i < g_routes.route_cnt; i++){
		if (strcasecmp(req->method, g_routes.route[i].method) == 0 && strcasecmp(req->route, g_routes.route[i].route) == 0){
			g_routes.route[i].cb(req);
		}
	}
}

static int on_message_begin(http_parser* _) {
	return 0;
}

static int on_headers_complete(http_parser* _) {
	return 0;
}

static int on_message_complete(http_parser* parser) {
	http_request * req = (http_request *) (parser->data);
	req->complete = 1;
	strcpy(req->method, http_method_str(parser->method));

	http_req_handle(req);
	return 0;
}

static int url_parser(http_request * req, char * url, size_t len) {
	char * route_start = url, *route_end = url, *url_par = NULL;
	char *token, *subtoken, *str1 = NULL, *saveptr1 = NULL, *str2 = NULL, *saveptr2 = NULL;

	if (!url) {
		return -1;
	}

	while (*route_end != '?') {
		if (route_end - route_start >= len) {
			break;
		}
		route_end++;
	}

	bzero(req->route, sizeof(req->route));
	memcpy(req->route, route_start, route_end - route_start);

	url_par = route_end + sizeof((char) '?');
	if (url_par - url >= len) {
		return -1;
	}

	for (str1 = url_par;; str1 = NULL) {
		token = strtok_r(str1, "&", &saveptr1);
		if (token == NULL) {
			break;
		}

		for (str2 = token;; str2 = NULL) {
			subtoken = strtok_r(str2, "=", &saveptr2);
			if (subtoken == NULL) {
				break;
			}

			strcpy(req->url_par.kv[req->url_par.cnt].key, subtoken);
			strcpy(req->url_par.kv[req->url_par.cnt].value, saveptr2);
			break;
		}

		log_imp("url par key=[%s], value: [%s]\n", req->url_par.kv[req->url_par.cnt].key, req->url_par.kv[req->url_par.cnt].value);
		req->url_par.cnt++;
	}
}

static int on_url(http_parser* parser, const char* at, size_t length) {
	http_request * req = (http_request *) parser->data;
	char * url = (char *) malloc(length + sizeof('\0'));

	bzero(url, length + sizeof('\0'));
	strncpy(url, at, length);

	url_parser(req, url, length);
	return 0;
}

static int on_header_field(http_parser* parser, const char* at, size_t length) {
	http_request * req = (http_request *) parser->data;
	strncpy(req->head.kv[req->head.cnt].key, at, length);
	return 0;
}

static int on_header_value(http_parser* parser, const char* at, size_t length) {
	http_request * req = (http_request *) parser->data;
	strncpy(req->head.kv[req->head.cnt].value, at, length);
	log_imp("Header key=[%s], value: [%s]\n", req->head.kv[req->head.cnt].key, req->head.kv[req->head.cnt].value);
	req->head.cnt++;
	return 0;
}

extern char * http_head_get(http_request *req, char *key) {
	int i = 0;

	for (i = 0; i < req->head.cnt; i++) {
		if (strcmp(key, req->head.kv[i].key) == 0) {
			return req->head.kv[i].value;
		}
	}
	return NULL;
}

static int on_body(http_parser* parser, const char* at, size_t length) {
	http_request * req = (http_request *)parser->data;
	char * content_type = http_head_get(req, "Content-Type");
	char * multipart_form_data = strstr(content_type, "multipart/form-data");
	char * boundary = strstr(content_type, "boundary=");
	boundary += strlen("boundary=");
	char * content_len = http_head_get(req, "Content-Length");
	int content_len_i = atoi(content_len);

//	log_imp("Body: %.*s\n", (int ) length, at);
	int write_len = 0;
	if (multipart_form_data && boundary){
		if (strlen(req->content.filename) == 0) {
			char *filename = strstr(at, "filename=");
			filename += strlen("filename=\"");
			int filename_len = strstr(filename, "\"\r\n") - filename;
			strncpy(req->content.filename, filename, filename_len);
			log_imp("filename=[%s]\n", req->content.filename);
			char *rn_rn = strstr(at, "\r\n\r\n");
			rn_rn += strlen("\r\n\r\n");
//			log_imp("rnrn=[%s], %ld\n", rn_rn, rn_rn - at);
			req->content.file_len = content_len_i - (rn_rn - at);
			req->content.file_len = req->content.file_len - strlen("\r\n");
			req->content.file_len = req->content.file_len - strlen("--") - strlen(boundary) - strlen("--") - strlen("\r\n");
			log_imp("file len=[%d]\n", req->content.file_len);
			remove(req->content.filename);
			req->content.file_fd = open(req->content.filename, O_CREAT | O_RDWR, 777);

			if (at + length >= rn_rn + req->content.file_len) {
				write_len = req->content.file_len;
				write(req->content.file_fd, rn_rn, write_len);
				req->content.file_writed += write_len;
				log_imp("write_len=[%d], file_writed=[%d],file_len=[%d]\n", write_len, req->content.file_writed, req->content.file_len);
			} else {
				write_len = at + length - rn_rn;
				write(req->content.file_fd, rn_rn, write_len);
				req->content.file_writed += write_len;
				log_imp("write_len=[%d], file_writed=[%d],file_len=[%d]\n", write_len, req->content.file_writed, req->content.file_len);
			}
		} else {
			if (req->content.file_writed + length < req->content.file_len) {
				write_len = length;
				write(req->content.file_fd, at, length);
				req->content.file_writed += write_len;
				log_imp("write_len=[%d], file_writed=[%d],file_len=[%d]\n", write_len, req->content.file_writed, req->content.file_len);
			} else {
				write_len = req->content.file_len - req->content.file_writed;
				write(req->content.file_fd, at, write_len);
				req->content.file_writed += write_len;
				log_imp("write_len=[%d], file_writed=[%d],file_len=[%d]\n", write_len, req->content.file_writed, req->content.file_len);
			}
		}
	} else {
		strncpy(req->content.body + req->content.body_len, at, length);
		req->content.body_len += length;
	}

	if (req->content.file_writed == req->content.file_len && req->content.file_fd > 0){
		close(req->content.file_fd);
		log_imp("file close\n");
	}
	return 0;
}

static void *server_thread(void *arg) {
	int fd = (int) arg;
	http_parser parser;
	http_parser_settings settings;
	http_request req;

	bzero(&req, sizeof(req));
	http_parser_init(&parser, HTTP_REQUEST);
	memset(&settings, 0, sizeof(settings));
	settings.on_message_begin = on_message_begin;
	settings.on_url = on_url;
	settings.on_header_field = on_header_field;
	settings.on_header_value = on_header_value;
	settings.on_headers_complete = on_headers_complete;
	settings.on_body = on_body;
	settings.on_message_complete = on_message_complete;

	req.fd = fd;
	req.complete = 0;
	parser.data = &req;
	while (!g_server_exit) {
		char recv_buf[4 * 1024] = { 0 };
		int recv_len = 0;

		recv_len = recv(fd, recv_buf, sizeof(recv_buf), 0);
		if (recv_len <= 0) {
			log_wrn("tcp is close\n");
			break;
		}
		http_parser_execute(&parser, &settings, recv_buf, recv_len);
		if (req.complete) {
			break;
		}
	}

	if (fd > 0) {
		close(fd);
	}

	return NULL;
}

extern int server_uninit() {
	__sync_lock_test_and_set(&g_server_exit, 1);
}

extern int server_init() {
	int fd = -1, server_addr_len = 0;
	struct sockaddr_in server_addr;

	bzero(&server_addr, sizeof(server_addr));

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	check_fd(fd);
	int on = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	server_addr_len = sizeof(server_addr);
	check_rst(bind(fd, (struct sockaddr * )&server_addr, server_addr_len));
	check_rst(listen(fd, 80));

	__sync_lock_test_and_set(&g_server_exit, 0);
	while (!g_server_exit) {
		int client_fd = 0;
		int client_addr_len = 0;
		struct sockaddr_in client_addr;
		fd_set rfds;
		struct timeval tv;
		int max_fd = fd;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		tv.tv_sec = 0;
		tv.tv_usec = 500 * 1000;

		int ret = select(max_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret > 0) {
			if (FD_ISSET(fd, &rfds)) {
				bzero(&client_addr, sizeof(client_addr));
				client_addr_len = sizeof(client_addr);

				client_fd = accept(fd, (struct sockaddr *) &client_addr, &client_addr_len);
				if (client_fd <= 0) {
					log_err("accept failed\n");
					goto end;
				}

				int client_port = 0;
				char client_ip[64] = { 0 };

				client_port = ntohs(client_addr.sin_port);
				inet_ntop(AF_INET, (void *) &client_addr.sin_addr, client_ip, sizeof(client_ip));
				log_imp("client connect to server, info ip=[%s], port=[%d]\n", client_ip, client_port);

				pthread_t pid = 0;
				pthread_create(&pid, NULL, server_thread, (void *) client_fd);
			}
		} else if (ret == 0) {

		} else {
			log_err("select error\n");
		}
	}
end: ;
	if (fd > 0) {
		close(fd);
	}
}
