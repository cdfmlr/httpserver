#include "in.h"
#include "rio.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// Uncomment this line to enable multiprocess
// #define MULTIPROCS 1

extern char **environ; /* Defined by libc */

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

// TODO: listen on :80 and :443 and redirect http to https
// TODO: add support for https (openssl)
int main(int argc, char **argv)
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
        
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
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

/* doit - handle one HTTP request/response transaction */
void doit(int fd)
{
    sleep(1); // sleep 1 second to test the concurrency

    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // Read request line and headers
    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "This method is not implemented");
        return;
    }
    read_requesthdrs(&rio);

    // Parse URI from GET request
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                    "Couldn't find this file");
        return;
    }

    if (is_static) { // Serve static content
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                        "Couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    } else { // Serve dynamic content
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                        "Couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

/* read_requesthdrs - read and parse HTTP request headers */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
}

/* parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) { // Static content
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "index.html");
        return 1;
    } else { // Dynamic content
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/* serve_static - copy a file back to the client */
// TODO: add support for range requests (status 206) (see RFC 7233)
void serve_static(int fd, char *filename, int filesize)
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
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    rio_writen(fd, srcp, filesize);
    munmap(srcp, filesize);
}

/* et_filetype - derive file type from file name */
void get_filetype(char *filename, char *filetype)
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
void serve_dynamic(int fd, char *filename, char *cgiargs)
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
        dup2(fd, STDOUT_FILENO); // Redirect stdout to client
        execve(filename, emptylist, environ); // Run CGI program
    }
    wait(NULL); // Parent waits for and reaps child
}

/* clienterror - returns an error message to the client */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
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
