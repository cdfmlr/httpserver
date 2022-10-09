#include <sys/socket.h>
#include <netinet/in.h>


// #define INADDR_ANY (unsigned long)0x00000000
#define LISTENQ 1024 /* second argument to listen() */
#define SA struct sockaddr

// /* Generic socket address structure (for connect, bind, and accept) */
// struct sockaddr {
//     unsigned short sa_family; /* address family */
//     char sa_data[14]; /* up to 14 bytes of direct address */
// };

// /* Internet address (a structure for historical reasons) */
// struct in_addr {
//     unsigned long s_addr; /* that's a 32-bit int (4 bytes) */
// };

// /* Internet-style socket address structure */
// struct sockaddr_in {
//     short sin_family; /* address family */
//     unsigned short sin_port; /* port number */
//     struct in_addr sin_addr; /* internet address */
//     char sin_zero[8]; /* unused */
// };

// struct hostent {
//     char *h_name; /* official name of host */
//     char **h_aliases; /* alias list */
//     int h_addrtype; /* host address type */
//     int h_length; /* length of address */
//     char **h_addr_list; /* list of addresses from name server */
// };

/* The following functions are defined in in.c */

/* open_listenfd - open and return a listening socket on port
 * Returns -1 and sets errno on Unix error. */
int open_listenfd(int port);

/* open_clientfd - open connection to server at <hostname, port>
 * and return a socket descriptor ready for reading and writing.
 * Returns -1 and sets errno on Unix error. */
int open_clientfd(char *hostname, int port);


