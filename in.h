#include <sys/socket.h>
#include <netinet/in.h>


// #define INADDR_ANY (unsigned long)0x00000000
#define LISTENQ 1024 /* second argument to listen() */
#define SA struct sockaddr

/* open_listenfd - open and return a listening socket on port
 * Returns -1 and sets errno on Unix error. */
int open_listenfd(int port);

/* open_clientfd - open connection to server at <hostname, port>
 * and return a socket descriptor ready for reading and writing.
 * Returns -1 and sets errno on Unix error. */
int open_clientfd(char *hostname, int port);
