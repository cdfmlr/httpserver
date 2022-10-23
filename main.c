#include <stdio.h>
#include "httpserver.h"

http_handler_static_func_new(file, "/Users/c/Learning/ucas2022fall/cn/httpserver/", "/static/");

http_handler_static_func_new(root_file_handler, "/Users/c/Learning/ucas2022fall/cn/all-exps/05-http_server", "/");


#define TLS_CERT_FILE "/Users/c/Learning/ucas2022fall/cn/all-exps/05-http_server/keys/cnlab.cert"
#define TLS_PKEY_FILE "/Users/c/Learning/ucas2022fall/cn/all-exps/05-http_server/keys/cnlab.prikey"

void
exp5() {
    // http

    http_server *http_to_https = http_server_new();

    http_server_add_handler(http_to_https,
                            http_handler_redirect_https_new("/"));

    if (!fork()) {
        return;
        http_server_start(http_to_https, 80, NULL);
    }

    // https

    http_server *server = http_server_new();

    http_server_add_handler(server,
                            http_handler_static_new("/", root_file_handler));

    tls_config tls = {
            .certificate_file=TLS_CERT_FILE,
            .private_key_file=TLS_PKEY_FILE,
    };

    // http_server_start(server, 4432, &tls);
    http_server_start(server, 4433, NULL);
}

int
main() {
    exp5();
    return 0;

    http_server *server = http_server_new();

    http_handler *echo = http_handler_echo_new("/");
    http_server_add_handler(server, echo);

    http_handler *not_found = http_handler_404_new("/404");
    http_server_add_handler(server, not_found);

    http_handler *file = http_handler_static_new("/static", file);
    http_server_add_handler(server, file);

    //    http_server_start(server, 8081, NULL);

    tls_config tls = {
            .certificate_file=TLS_CERT_FILE,
            .private_key_file=TLS_PKEY_FILE,
    };

    http_server_start(server, 8083, &tls);

    http_server_free(server);
}
