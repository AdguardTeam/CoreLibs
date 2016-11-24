cmake_minimum_required(VERSION 3.0)
project(http-parser)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/parser.h
        src/parser.c
        src/nodejs_http_parser/http_parser.h
        src/nodejs_http_parser/http_parser.c)

add_library(httpparser-c ${SOURCE_FILES})
set_property(TARGET httpparser-c PROPERTY POSITION_INDEPENDENT_CODE ON)

enable_testing()

add_executable(test_http_parse test/test_http_parse.c)
add_executable(test_decode test/test_decode.c)

add_test(http_parse test_http_parse)
add_test(decode test_decode)
