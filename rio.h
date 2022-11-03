/*
 * rio: The Robust I/O Lib
 *
 * from: CS:APP2e (http://csapp.cs.cmu.edu/2e/home.html)
 * by:   Randal E. Bryant and David R. O'Hallaron, Carnegie Mellon University
 *
 * modified: CDFMLR 2022-10-18:
 *
 *   Add a concept called ReadWriter,
 *   to make it working with different I/O (the unix file, Open SSL, ...)
 *
 */

#ifndef HTTPSERVER_RIO_H
#define HTTPSERVER_RIO_H

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvoid-pointer-to-int-cast"

#define MAXBUF 8192
#define MAXLINE 8192
#define RIO_BUFSIZE 8192
#define EINTR 4

#ifndef READWRITER
#define READWRITER

typedef struct read_writer {
    ssize_t (*reader)(void *fd, void *buf, size_t n);

    ssize_t (*writer)(void *fd, void *buf, size_t n);
} ReadWriter;

#endif //READWRITER

#ifndef FD_READWRITER
#define FD_READWRITER

static inline ssize_t fd_reader(void *fd, void *buf, size_t n) {
    return read((int) fd, buf, n);
}

static inline ssize_t fd_writer(void *fd, void *buf, size_t n) {
    return write((int) fd, buf, n);
}

// The normal unistd read/write
static ReadWriter fd_rw = {.reader=&fd_reader, .writer=&fd_writer};

// The normal unistd read/write ReadWriter. Ready to use
#define the_fd_rw (&fd_rw)

#endif //FD_READWRITER

/* Persistent state for the robust I/O (Rio) package */
typedef struct rio_t {
    void *rio_fd; /* descriptor for this internal buf */
    int rio_cnt; /* unread bytes in internal buf */
    char *rio_bufptr; /* next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* internal buffer */

    ReadWriter *rw;   /* r/w interface */
} rio_t;

/* rio_readinitb - associate a descriptor with a read buffer and reset buffer */
void rio_readinitb(rio_t *rp, void *fd, ReadWriter *rw);

/* rio_read - This is a wrapper for the Unix read() function that
 * transfers min(n, rp->rio_cnt) bytes from an internal buffer to a user
 * buffer, where n is the number of bytes requested by the user and
 * rp->rio_cnt is the number of unread bytes in the internal buffer. On
 * entry, rio_read() refills the internal buffer via a call to read()
 * if the internal buffer is empty.
 */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);

/* rio_readnb - robustly read n bytes (unbuffered) */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

/* rio_readlineb - robustly read a text line (buffered) */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);

/* rio_writen - robustly write n bytes (unbuffered) */
void rio_writen(ReadWriter *rw, void *fd, void *usrbuf, size_t n);


#pragma clang diagnostic pop

#endif //HTTPSERVER_RIO_H
