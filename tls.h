//
// Created by c on 2022/10/18.
//

#ifndef HTTPSERVER_TLS_H
#define HTTPSERVER_TLS_H

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "rio.h"

// reference: https://wiki.openssl.org/index.php/Simple_TLS_Server

// create_context creates a OpenSSL context
SSL_CTX *create_context();

// configure_context sets the key and cert
void configure_context(SSL_CTX *ctx, char *cert_file, char *key_file);

#ifndef SSL_READWRITER
#define SSL_READWRITER

static inline ssize_t ssl_reader(void *fd, void *buf, size_t n) {
    return SSL_read(fd, buf, n);
}

static inline ssize_t ssl_writer(void *fd, void *buf, size_t n) {
    return SSL_write(fd, buf, n);
}

// r/w to SSL.
static ReadWriter ssl_rw = {.reader=&ssl_reader, .writer=&ssl_writer};

// ReadWriter r/w to SSL. Ready to use.
#define the_ssl_rw (&ssl_rw)

#endif //SSL_READWRITER

#endif //HTTPSERVER_TLS_H
