#include "httpserver.h"

#include <time.h>

/******************* http_request *******************/

// Struct for HTTP request
// buf_chars is the buffer size for reading the request in bytes
http_request *
http_request_new() {
    http_request *req = calloc(1, sizeof(http_request));
    // 借用 calloc 的 zero-filled 特性，避免 malloc 给的一些奇怪的未初始化内存带来的问题。

    req->headers = kvs_new();
    // req->queries = kvs_new();
    req->body = NULL;
    return req;
}

void
http_request_free(http_request *req) {
    kvs_free(req->headers);
    // kvs_free(req->queries);
    if (req->body) {
        free(req->body);
    }
    free(req);
}

// helper for http_request_read:
//
// copy a str char-by-char
// from buffer (char *buf) to dst (another char *)
// until meeting some char (char until).
#define parse_and_copy_str(buf, dst, until) { \
    while (*(buf) != (until)) {               \
        *(dst)++ = *(buf)++;                  \
    }                                         \
    *(dst) = '\0';                            \
    (buf)++;                                  \
}

/*
 * Parse HTTP request from client... I hope it works.
 * MAGIC! DO NOT TOUCH!
 */
void
http_request_read(rio_t *rp, http_request *req) {
    char *buf = calloc(MAXLINE, sizeof(char));
    int i = 0;

    // Parse request line: GET /path HTTP/1.1
    // stack values: copy
    rio_readlineb(rp, buf, MAXLINE);
    char *p = buf;

    // method
    char *q = req->method;
    parse_and_copy_str(p, q, ' ');

    // uri
    q = req->uri;
    parse_and_copy_str(p, q, ' ');

    // version
    q = req->version;
    parse_and_copy_str(p, q, '\r');
    p++;

    // Parse headers
    char *key, *value;

    for (i = 0; i < MAXLINENR; i++) {
        rio_readlineb(rp, buf, MAXLINE);
        p = buf;

        if (*p == '\r') { // skip \r\n
            p += 2;
            break;
        }

        key = p;
        while (*p != ':') {
            p++;
        }
        *p = '\0';

        value = ++p;
        while (*p != '\r') {
            p++;
        }
        *p = '\0';
        p += 2; // skip \r\n

        // skip leading space
        while (*value == ' ') {
            value++;
        }

        // add header to headers kvs
        kvs_set(req->headers, key, value);
    }

    // Parse body
    p = kvs_get(req->headers, "Content-Length");
    if (p != NULL) {
        int len = atoi(p);
        if (len <= 0) {
            return;
        }
        req->body = malloc(len + 1);
        rio_readnb(rp, req->body, len);
        req->body[len] = '\0';
    }
}

/******************* http_response *******************/

http_response *
http_response_new() {
    http_response *res = calloc(1, sizeof(http_response));
    res->headers = kvs_new();
    res->body = NULL;
    res->body_munmap_len = -1;
    return res;
}

void
http_response_free(http_response *res) {
    kvs_free(res->headers);
    if (res->body) {
        // free the malloced body or munmap the mmaped body.
        // see comments to body_munmap_len.
        if (res->body_munmap_len <= 0) {
            free(res->body);
        } else {
            munmap(res->body, res->body_munmap_len);
        }
    }
    free(res);
}

void
http_response_send(ReadWriter *rw, void *fd, http_response *res) {
    char buf[MAXLINE];
    char *p, *q;
    int i;

    // Send status line
    sprintf(buf, "%s %d %s\r\n", res->version, res->status, res->reason);
    rio_writen(rw, fd, buf, strlen(buf));

    // the length of body
    size_t body_len = res->body_munmap_len;

    // Send headers
    for (kv *kv = res->headers->head; kv != NULL; kv = kv->next) {
        if (strcmp(kv->key, "Content-Length") == 0) {
            body_len = atoi(kv->value);
        }
        sprintf(buf, "%s: %s\r\n", kv->key, kv->value);
        rio_writen(rw, fd, buf, strlen(buf));
    }
    rio_writen(rw, fd, "\r\n", 2);

    if (body_len < 0) {
        body_len = strlen(res->body);
    }

    // Send body
    if (res->body != NULL) {
        rio_writen(rw, fd, res->body, body_len);
    }
}

void
http_response_ok(http_response *res) {
    strcpy(res->version, "HTTP/1.0");
    res->status = 200;
    strcpy(res->reason, "OK");
}

void
http_response_text_body(http_response *res, char *body) {
    kvs_set(res->headers, "Content-Type", "text/plain");
    char l[32];
    sprintf(l, "%lu", strlen(body));
    kvs_set(res->headers, "Context-Length", l);

    res->body = malloc(strlen(body) + 1);
    strcpy(res->body, body);
}

/******************* http_handler *******************/

http_handler *
http_handler_new(char *prefix, void (*handler)(http_request *, http_response *)) {
    http_handler *h = malloc(sizeof(http_handler));
    h->prefix = prefix;
    h->handler = handler;
    return h;
}

void
http_handler_free(http_handler *h) {
    if (h) {
        free(h);
    }
}

/******************* http_server *******************/

http_server *
http_server_new() {
    http_server *server = malloc(sizeof(http_server));
    server->head = NULL;
    server->nhandlers = 0;
    return server;
}

void
http_server_free(http_server *server) {
    http_handler *h = server->head;
    while (h != NULL) {
        http_handler *next = h->next;
        http_handler_free(h);
        h = next;
    }
    free(server);
}

// Add a handler to the server, sorted by prefix length
void
http_server_add_handler(http_server *server, http_handler *h) {
    http_handler *prev = NULL;
    http_handler *cur = server->head;
    while (cur != NULL && strlen(cur->prefix) > strlen(h->prefix)) {
        prev = cur;
        cur = cur->next;
    }
    if (prev == NULL) {
        server->head = h;
    } else {
        prev->next = h;
    }
    h->next = cur;
    server->nhandlers++;
}

// Find the handler for the given uri.
// The longest prefix match is returned or NULL if no match is found.
// TODO: path prefix != string prefix
http_handler *
http_server_find_handler(http_server *server, char *uri) {
    http_handler *h = server->head;
    while (h != NULL) {
        if (strncmp(uri, h->prefix, strlen(h->prefix)) == 0) {
            return h;
        }
        h = h->next;
    }
    return NULL;
}

typedef struct inner_http_ctx_t {
    void *rfd, *wfd;
    ReadWriter *rw;
    tls_config *tls;
} inner_http_ctx_t;

// (private) get current time in str
static inline char *current_time() {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char *time_str = asctime(timeinfo);
    time_str[strlen(time_str) - 1] = '\0';
    return time_str;
}

// (private) prints a log for a http request/response
// It is a part of http_server_serve_http.
// We do not support middlewares (这个也简单，但暂时懒得写). TODO: middlewares
static inline void log_http(http_request *req, http_response *res) {
    printf("%s: %s %s -> [%d]\n", current_time(), req->method, req->uri, res->status);
}

// (private) Handle a single HTTP request
// rfd, wfd: the read channel and the write channel,
// which can be set independently. Common to be the same one
static void
http_server_serve_http(http_server *server, inner_http_ctx_t *c) {
    http_request *req = http_request_new();
    http_response *res = http_response_new();

    rio_t rio;
    rio_readinitb(&rio, c->rfd, c->rw);

    http_request_read(&rio, req);

    // Find handler
    http_handler *h = http_server_find_handler(server, req->uri);
    if (h != NULL) {
        h->handler(req, res);
    } else {
        http_handler_404(req, res);
    }

    http_response_send(c->rw, c->wfd, res);

    log_http(req, res);

    http_request_free(req);
    http_response_free(res);
}

// (private) Handle a single HTTPS request
static void
http_server_serve_https(http_server *server, inner_http_ctx_t *c) {
    SSL_CTX *ctx = create_context();
    configure_context(ctx, c->tls->certificate_file, c->tls->private_key_file);

    SSL *ssl = SSL_new(ctx);

    // SSL_set_fd(ssl, fd);
    SSL_set_rfd(ssl, c->rfd);
    SSL_set_wfd(ssl, c->wfd);

    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        // TODO: connection checking
        // client could close the connection before we go:
        //   curl failed to verify the legitimacy of the server
        //   and therefore could not establish a secure connection to it.

        // rewrite context
        c->rfd = c->wfd = ssl;
        c->rw = the_ssl_rw;

        http_server_serve_http(server, c);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
}

// (private) MUX: HTTP or HTTPS
static void
http_server_serve(http_server *server, int fd, tls_config *tls_config) {
    inner_http_ctx_t c = {
            .rfd=fd,
            .wfd=fd,
            .tls=tls_config,
            .rw=the_fd_rw,
    };

    if (tls_config != NULL) {
        http_server_serve_https(server, &c);
    } else {
        http_server_serve_http(server, &c);
    }
}

// Start the server
void
http_server_start(http_server *server, int port, tls_config *tls_config) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    listenfd = open_listenfd(port);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = accept(listenfd, (SA *) &clientaddr, &clientlen);
        if (connfd < 0) {
            continue;
        }

#ifdef MULTIPROCS
        if (fork() == 0) {
            close(listenfd);
            http_server_serve(server, connfd);
            close(connfd);
            exit(0);
        }
        close(connfd);
#else
        http_server_serve(server, connfd, tls_config);
        close(connfd);
#endif
    }
}

/******************* handlers *******************/

// A simple handler func that returns the request URI
void
http_handler_echo(http_request *req, http_response *res) {
    http_response_ok(res);
    http_response_text_body(res, req->uri);
}

// 404 handler func
void
http_handler_404(http_request *req, http_response *res) {
    strcpy(res->version, "HTTP/1.0");
    res->status = 404;
    strcpy(res->reason, "Not Found");

    http_response_text_body(res, "404 Not Found");
}

// 301 moved permanently: HTTP -> HTTPS
void
http_handler_redirect_https(http_request *req, http_response *res) {
    strcpy(res->version, req->version);
    res->status = 301;
    strcpy(res->reason, "Moved Permanently");

    // Location: "https://" + host + path

    char *host = kvs_get(req->headers, "Host");
    char *path = req->uri;

    size_t len = strlen(host) + strlen(path) + 8;
    char *s = malloc(len);

    sprintf(s, "https://%s%s", host, path);

    kvs_set(res->headers, "Location", s);

    free(s);  // kvs_set makes a copy
}

// static file handler func
void
_http_handler_static(char *base_dir,
                     char *uri_prefix,
                     http_request *req,
                     http_response *res) {
    strcpy(res->version, req->version);

    char path[MAXLINE];
    struct stat sbuf;

    sprintf(path, "%s/%s", base_dir, req->uri + strlen(uri_prefix));
    printf("_http_handler_static: path=%s\n", path);

    if (stat(path, &sbuf) < 0) {
        http_handler_404(req, res);
        return;
    }

    if (S_ISDIR(sbuf.st_mode)) {
        sprintf(path, "%s/index.html", path);
        if (stat(path, &sbuf) < 0) {
            http_handler_404(req, res);
            return;
        }
    }

    off_t start_off = 0;
    size_t len = sbuf.st_size;

    // Range: RFC 7233
    char range_flag = 0;
    char *range = kvs_get(req->headers, "Range");
    if (range != NULL) {
        range_flag = 1;
        if (strncmp(range, "bytes=", 6) != 0) {
            res->status = 400;
            strcpy(res->reason, "Bad Request");

            kvs_set(res->headers, "Content-Type", "text/plain");
            res->body = "400 Bad Request\nRange: only support bytes\n";
            return;
        }
        range += 6;
        char *p = strchr(range, '-');
        if (p != NULL) {
            *p = '\0';
            start_off = atoi(range);
            if (p[1] != '\0') {
                len = atoi(p + 1) - start_off + 1;
            }
        }
    }

    int fd = open(path, O_RDONLY, 0);

    void *srcp = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, start_off);

    // NOTE! use munmap to free mmap
    res->body_munmap_len = len;

    res->body = srcp;

    close(fd);

    if (range_flag) {
        res->status = 206;
        strcpy(res->reason, "Partial Content");

        char content_range[MAXLINE];
        sprintf(content_range,
                "bytes %lld-%llu/%lld",
                start_off,
                start_off + len - 1,
                sbuf.st_size);
        kvs_set(res->headers, "Content-Range", content_range);
    } else {
        res->status = 200;
        strcpy(res->reason, "OK");
    }

    char *filetype = mime_type(path);
    kvs_set(res->headers, "Content-Type", filetype);

    char content_len[20];
    sprintf(content_len, "%ld", len);
    kvs_set(res->headers, "Content-Length", content_len);
}

// Get the mime type of file.
// Return a static string to avoid memory leak
char *
mime_type(char *filename) {
    if (strstr(filename, ".html")) {
        return "text/html";
    } else if (strstr(filename, ".css")) {
        return "text/css";
    } else if (strstr(filename, ".js")) {
        return "application/javascript";
    } else if (strstr(filename, ".png")) {
        return "image/png";
    } else if (strstr(filename, ".jpg")) {
        return "image/jpeg";
    } else if (strstr(filename, ".gif")) {
        return "image/gif";
    } else if (strstr(filename, ".ico")) {
        return "image/x-icon";
    } else if (strstr(filename, ".pdf")) {
        return "application/pdf";
    } else if (strstr(filename, ".zip")) {
        return "application/zip";
    } else if (strstr(filename, ".gz")) {
        return "application/gzip";
    } else if (strstr(filename, ".mp4")) {
        return "video/mp4";
    } else if (strstr(filename, ".mp3")) {
        return "audio/mpeg";
    } else {
        return "text/plain";
    }
}
