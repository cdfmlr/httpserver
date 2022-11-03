/*
 * httpserver: A simple (toy) web framework.
 *
 * By: CDFMLR 2022
 *
 * Features:
 * - HTTP/HTTPS supports
 * - structured Request & Response
 * - custom handler function
 * - static file service
 * - CGI (WIP)
 *
 * How it works:
 *
 *                                       -> http_request  ->
 * socket <-> (OpenSSL) <->  http_server                     http_handler
 *                                       <- http_response <-
 *
 * | in.h  |    tls.h    |               httpserver.h                     |
 * |      rio.h         <->                                               |
 *
 *
 */

#ifndef HTTPSERVER_HTTPSERVER_H
#define HTTPSERVER_HTTPSERVER_H

#include "in.h"
#include "kv.h"
#include "rio.h"
#include "tls.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

// Uncomment this line to enable multiprocess
#define MULTIPROCS 1

#define MAXMETHOD 10
#define MAXURI MAXLINE
#define MAXVERSION 10

#define MAXREASON 32

#define MAXLINENR 128

/******************* http_request *******************/

// Struct for HTTP request
typedef struct http_request {
    char method[10];
    char uri[MAXURI];
    char version[MAXVERSION];

    kvs *headers;
    // kvs* queries;  // 暂时不做了，没必要

    char *body;
} http_request;

http_request *
http_request_new();

void
http_request_free(http_request *req);

void
http_request_read(rio_t *rp, http_request *req);

/******************* http_response *******************/

// Struct for HTTP response
typedef struct http_response {
    char version[MAXVERSION];
    int status;
    char reason[MAXREASON];

    kvs *headers;
    void *body;

    // For the fact that there are 3 kinds of body:
    //
    //  1. NULL
    //  2. allocated by malloc (most cases)
    //  3. allocated by mmap (http_handler_static use this to speed up)
    //
    // body_munmap_len is both a flag to identify body is malloced or mmaped,
    // and the length arg to munmap.
    //
    // body_munmap_len <= 0 if body is NULL or allocated by malloc
    // body_munmap_len should be set to length of body (>0) allocated by mmap.
    size_t body_munmap_len;

} http_response;

http_response *
http_response_new();

void
http_response_free(http_response *res);

void
http_response_send(ReadWriter *rw, void *fd, http_response *res);

// http_response_ok writes status line: HTTP/1.0 200 OK
void
http_response_ok(http_response *res);

// http_response_text_body writes a string as body with:
//   Content-Type: text/plain
//   Context-Length: 自动计算
void
http_response_text_body(http_response *res, char *body);

/******************* http_handler *******************/

// Function for HTTP handlers
typedef void (*http_handler_func)(http_request *, http_response *);

// Struct for HTTP handler
typedef struct http_handler {
    char *prefix;

    void (*handler)(http_request *, http_response *);

    struct http_handler *next; // next handler in the server's handler list, NOT
    // for chaining (i.e. not for Middlewares)
} http_handler;

http_handler *
http_handler_new(char *prefix, void (*handler)(http_request *, http_response *));

void
http_handler_free(http_handler *h);

/******************* http_server *******************/

// Struct for HTTP server
typedef struct http_server {
    http_handler *head;
    int nhandlers;
} http_server;

typedef struct tls_config {
    char *certificate_file;
    char *private_key_file;
} tls_config;

http_server *
http_server_new();

void
http_server_free(http_server *server);

// Add a handler to the server, sorted by prefix length
void
http_server_add_handler(http_server *server, http_handler *h);

// Find the handler for the given uri.
// The longest prefix match is returned or NULL if no match is found.
http_handler *
http_server_find_handler(http_server *server, char *uri);

// Start the server on port. HTTPS if tls_config is not NULL.
void
http_server_start(http_server *server, int port, tls_config *tls_config);

/******************* handlers *******************/

// A simple handler func that returns the request URI
void
http_handler_echo(http_request *req, http_response *res);

#define http_handler_echo_new(prefix)                                          \
    http_handler_new(prefix, http_handler_echo)

// 404 handler func
void
http_handler_404(http_request *req, http_response *res);

#define http_handler_404_new(prefix) http_handler_new(prefix, http_handler_404)

// 301 moved permanently: HTTP -> HTTPS
void
http_handler_redirect_https(http_request *req, http_response *res);

#define http_handler_redirect_https_new(prefix) http_handler_new(prefix, http_handler_redirect_https)

// static file handler func
void
_http_handler_static(char *base_dir,
                     char *uri_prefix,
                     http_request *req,
                     http_response *res);

// make a static file handler func:
//    http_handler_static_func(name) => http_handler_static_name_func
// NOTE: 这里要用宏做一个“偏函数”，只能顶层调用，不能放到函数里面！！！
//       暂时没有想到更好的方法。
#define http_handler_static_func_new(name, base_dir, uri_prefix)               \
    void http_handler_static_##name##_func(http_request* req,                  \
                                           http_response* res)                 \
    {                                                                          \
        _http_handler_static(base_dir, uri_prefix, req, res);                  \
    }

// get a static file handler func: http_handler_static_name_func
#define http_handler_static_func(name) http_handler_static_##name##_func

// make a static file handler.
// 先在全局用 http_handler_static_func_new 制造偏函数
// 再用 http_handler_static_new 将对应函数包装为 http_handler
#define http_handler_static_new(prefix, func_name)                    \
    http_handler_new(prefix, http_handler_static_func(func_name))

// TODO: CGI handler func!
//       supporting cgi could be fun.

// Get the mime type of file.
// Return a static string to avoid memory leak
char *
mime_type(char *filename);

extern char **environ; /* Defined by libc */

#endif //HTTPSERVER_HTTPSERVER_H