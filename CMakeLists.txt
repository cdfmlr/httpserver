cmake_minimum_required(VERSION 3.23)
project(httpserver C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(httpserver
        bufio.h
        httpserver.c
        httpserver.h
        in.c
        in.h
        kv.c
        kv.h
#        kv_test.c
        main.c
        rio.c
        rio.h)
