#include "httpserver.h"

#define TLS_CERT_FILE                                                          \
    "/Users/c/Learning/ucas2022fall/cn/all-exps/05-http_server/keys/"          \
    "cnlab.cert"
#define TLS_PKEY_FILE                                                          \
    "/Users/c/Learning/ucas2022fall/cn/all-exps/05-http_server/keys/"          \
    "cnlab.prikey"

http_handler_static_func_new(s,
                             "/Users/c/Learning/ucas2022fall/cn/all-exps/05-http_server",
                             "/static/");

void
exp_echo()
{
    http_server* server = http_server_new();

    http_handler* echo = http_handler_echo_new("/echo/");
    http_server_add_handler(server, echo);

    if (!fork()) {
        http_server_start(server, 20010, NULL);
    }

    tls_config tls = { TLS_CERT_FILE, TLS_PKEY_FILE };

    http_server_start(server, 20011, &tls);
}

void exp_file()
{
    http_server* server = http_server_new();

    http_server_add_handler(server, http_handler_static_new("/static", s));

    if (!fork()) {
        http_server_start(server, 20110, NULL);
    }

    tls_config tls = { TLS_CERT_FILE, TLS_PKEY_FILE };

    http_server_start(server, 20111, &tls);
}

void exp_perf() {
    http_server* server = http_server_new();

    http_server_add_handler(server, http_handler_static_new("/static", s));

    if (!fork()) {
        http_server_start(server, 20210, NULL);
        // http_server_start(server, 20219, NULL);
    }

    tls_config tls = { TLS_CERT_FILE, TLS_PKEY_FILE };

    http_server_start(server, 20211, &tls);
}

int
main()
{
    // if (!fork()) {
    //     exp_echo();
    // }
    // if(!fork()) {
    //     exp_file();
    // }
    if(!fork()) {
        exp_perf();
    }
    pause();
}
