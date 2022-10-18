#include <stdio.h>
#include "httpserver.h"

http_handler_static_func_new(file, "/Users/c/Learning/ucas2022fall/cn/httpserver/", "/static/");

int
main() {
    http_server *server = http_server_new(8081);

    http_handler *echo = http_handler_echo_new("/");
    http_server_add_handler(server, echo);

    http_handler *not_found = http_handler_404_new("/404");
    http_server_add_handler(server, not_found);

    http_handler *file = http_handler_static_new("/static", file);
    http_server_add_handler(server, file);

    http_server_start(server);

    http_server_free(server);
}
