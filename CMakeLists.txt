cmake_minimum_required(VERSION 3.23)
project(httpserver C)

set(CMAKE_C_COMPILER gcc-11)
set(CMAKE_C_STANDARD 11)

include_directories(.)

find_package(OpenSSL REQUIRED)

set(INC_DIR /usr/local/opt/openssl@3/include)
set(LINK_DIR /usr/local/opt/openssl@3/lib)

include_directories(${INC_DIR})
link_directories(${LINK_DIR})
#link_libraries(openssl)

add_executable(httpserver
        bufio.h
        httpserver.c
        httpserver.h
        in.c
        in.h
        kv.c
        kv.h
        #kv_test.c
        main.c
        rio.c
        rio.h tls.h tls.c)

add_executable(exp
        bufio.h
        httpserver.c
        httpserver.h
        in.c
        in.h
        kv.c
        kv.h
        #kv_test.c
        #main.c
        exp.c
        rio.c
        rio.h tls.h tls.c)

set(DEP_LIBS
        OpenSSL::Crypto OpenSSL::SSL  # modern
        #${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}  # old style
        )

target_link_libraries(httpserver ${DEP_LIBS})
target_link_libraries(exp ${DEP_LIBS})