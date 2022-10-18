#include "httpserver.h"
// #include "kv.h"

/******************* http_request *******************/

// Struct for HTTP request
// buf_chars is the buffer size for reading the request in bytes
http_request *
http_request_new() {
    http_request *req = malloc(sizeof(http_request));
    req->headers = kvs_new();
    // req->queries = kvs_new();
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

#define parse_and_copy_str(buf, dst, until) { \
    while (*(buf) != (until)) {               \
        *(dst)++ = *(buf)++;                  \
    }                                         \
    *(dst) = '\0';                            \
    (buf)++;                                  \
}

/*
 * Parse HTTP request from client
 * MAGIC! DO NOT TOUCH!
 */
void
http_request_read(rio_t *rp, http_request *req) {
    char *buf = malloc(MAXLINE);
    int i = 0;

    // Parse request line: GET /path HTTP/1.1
    // stack values: copy
    rio_readlineb(rp, buf, MAXLINE);
    char *p = buf;

    // method
    char *q = req->method;
    // while (*p != ' ') {
    //    *q++ = *p++;
    // }
    // *q = '\0';
    // p++;
    parse_and_copy_str(p, q, ' ');

    // uri
    q = req->uri;
    // while (*p != ' ') {
    //    *q++ = *p++;
    // }
    // *q = '\0';
    // p++;
    parse_and_copy_str(p, q, ' ');

    // version
    q = req->version;
    //    while (*p != '\r') {
    //        *q++ = *p++;
    //    }
    //    *q = '\0';
    //    p += 2;
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

        // add header to headers kvs
        kvs_set(req->headers, key, value);
    }

    // // Parse queries
    // p = req->uri;
    // while (*p != '\0') {
    //     if (*p == '?') {
    //         *p = '\0';
    //         p++;
    //         break;
    //     }
    //     p++;
    // }
    // while (*p != '\0') {
    //     q = p;
    //     while (*q != '\0' && *q != '&') {
    //         q++;
    //     }
    //     if (*q == '&') {
    //         *q = '\0';
    //         q++;
    //     }
    //     i = 0;
    //     while (p[i] != '\0' && p[i] != '=') {
    //         i++;
    //     }
    //     if (p[i] == '=') {
    //         p[i] = '\0';
    //         kvs_set(req->queries, p, p + i + 1);
    //     }
    //     p = q;
    // }

    // Parse body
    p = kvs_get(req->headers, "Content-Length");
    if (p != NULL) {
        int len = atoi(p);
        req->body = malloc(len + 1);
        rio_readnb(rp, req->body, len);
        req->body[len] = '\0';
    }
}

/******************* http_response *******************/

http_response *
http_response_new() {
    http_response *res = malloc(sizeof(http_response));
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
http_response_send(int fd, http_response *res) {
    char buf[MAXLINE];
    char *p, *q;
    int i;

    // Send status line
    sprintf(buf, "%s %d %s\r\n", res->version, res->status, res->reason);
    rio_writen(fd, buf, strlen(buf));

    // Send headers
    for (kv *kv = res->headers->head; kv != NULL; kv = kv->next) {
        sprintf(buf, "%s: %s\r\n", kv->key, kv->value);
        rio_writen(fd, buf, strlen(buf));
    }
    rio_writen(fd, "\r\n", 2);

    // Send body
    if (res->body != NULL) {
        rio_writen(fd, res->body, strlen(res->body));
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
http_server_new(int port) {
    http_server *server = malloc(sizeof(http_server));
    server->port = port;
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

// Start the server
void
http_server_start(http_server *server) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    listenfd = open_listenfd(server->port);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = accept(listenfd, (SA *) &clientaddr, &clientlen);

#ifdef MULTIPROCS
        if (fork() == 0) {
            close(listenfd);
            http_server_serve(server, connfd);
            close(connfd);
            exit(0);
        }
        close(connfd);
#else
        http_server_serve(server, connfd);
        close(connfd);
#endif
    }
}

// Handle a single HTTP request
void
http_server_serve(http_server *server, int fd) {
    http_request *req = http_request_new();
    http_response *res = http_response_new();

    rio_t rio;
    rio_readinitb(&rio, fd);

    http_request_read(&rio, req);

    // Find handler
    http_handler *h = http_server_find_handler(server, req->uri);
    if (h != NULL) {
        h->handler(req, res);
    } else {
        http_handler_404(req, res);
    }

    http_response_send(fd, res);

    http_request_free(req);
    http_response_free(res);
}

/******************* handlers *******************/

// A simple handler func that returns the request URI
void
http_handler_echo(http_request *req, http_response *res) {
    //    res->version = "HTTP/1.0";
    //    res->status = 200;
    //    res->reason = "OK";
    //
    //    kvs_set(res->headers, "Content-Type", "text/plain");
    //    char l[32];
    //    sprintf(l, "%lu", strlen(req->uri));
    //    kvs_set(res->headers, "Context-Length", l);
    //
    //    res->body = malloc(strlen(req->uri) + 1);
    //    strcpy(res->body, req->uri);
    http_response_ok(res);
    http_response_text_body(res, req->uri);
}

// 404 handler func
void
http_handler_404(http_request *req, http_response *res) {
    strcpy(res->version, "HTTP/1.0");
    res->status = 404;
    strcpy(res->reason, "Not Found");

    //    kvs_set(res->headers, "Content-Type", "text/plain");
    //    char *s = "404 Not Found";
    //    res->body = malloc(strlen(s) + 1);
    //    strcpy(res->body, s);
    //
    //    char l[32];
    //    sprintf(l, "%lu", strlen(req->uri));
    //    kvs_set(res->headers, "Context-Length", l);
    http_response_text_body(res, "404 Not Found");
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
    // NOTE! use munmap to free mmap
    char *srcp = (char *) mmap(0, len, PROT_READ, MAP_PRIVATE, fd, start_off);
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

    res->body = srcp;
}

// Get the mime type of a file.
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

/******************* legacy code *******************/

// The following code is from the book. TO BE REMOVED.

#ifdef LEGACY

// TODO: listen on :80 and :443 and redirect http to https
// TODO: add support for https (openssl)
int
old_main(int argc, char** argv)
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    char hostname[MAXLINE], portnum[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    if (listenfd < 0) {
        fprintf(stderr, "open_listenfd error: %s\n", strerror(errno));
        exit(1);
    }

    while (1) {
        clientlen = sizeof(clientaddr);

        connfd = accept(listenfd, (SA*)&clientaddr, &clientlen);
        if (connfd < 0) {
            fprintf(stderr, "accept error: %s (%d)\n", strerror(errno), errno);
        }

#ifdef MULTIPROCS
        if (fork() == 0) {
            close(listenfd);
            doit(connfd);
            close(connfd);
            exit(0);
        } else {
            close(connfd);
        }
#else
        doit(connfd);
        close(connfd);
#endif
    }

    return 0;
}

/* serve_static - copy a file back to the client */
void
serve_static(int fd, char* filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    // Send response headers to client
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    rio_writen(fd, buf, strlen(buf));

    // Send response body to client
    srcfd = open(filename, O_RDONLY, 0);

    // help: man 2 mmap
    srcp = mmap(
      0, filesize /* len */, PROT_READ, MAP_PRIVATE, srcfd, 0 /* offset */);
    close(srcfd);
    rio_writen(fd, srcp, filesize);
    munmap(srcp, filesize);
}

/* et_filetype - derive file type from file name */
void
get_filetype(char* filename, char* filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

/* serve_dynamic - run a CGI program on behalf of the client */
void
serve_dynamic(int fd, char* filename, char* cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    // Return first part of HTTP response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    rio_writen(fd, buf, strlen(buf));

    if (fork() == 0) { // Child
        // Real server would set all CGI vars here
        setenv("QUERY_STRING", cgiargs, 1);
        dup2(fd, STDOUT_FILENO);              // Redirect stdout to client
        execve(filename, emptylist, environ); // Run CGI program
    }
    wait(NULL); // Parent waits for and reaps child
}

/* clienterror - returns an error message to the client */
void
clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    // Build the HTTP response body
    sprintf(body, "%s<h1>%s: %s</h1>\r\n", body, errnum, shortmsg);
    sprintf(body, "%s%s: %s\r\n", body, longmsg, cause);

    // Print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

#endif