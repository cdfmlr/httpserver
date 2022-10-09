#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

#define MAXBUF 8192
#define MAXLINE 8192
#define RIO_BUFSIZE 8192
#define EINTR 4

/* Persistent state for the robust I/O (Rio) package */
typedef struct rio_t {
    int rio_fd; /* descriptor for this internal buf */
    int rio_cnt; /* unread bytes in internal buf */
    char *rio_bufptr; /* next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* internal buffer */
} rio_t;

/* rio_readinitb - associate a descriptor with a read buffer and reset buffer */
void rio_readinitb(rio_t *rp, int fd);

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
void rio_writen(int fd, void *usrbuf, size_t n);
