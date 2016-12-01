cmake_minimum_required(VERSION 3.0)
project(http-parser)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -O0 -g3 -ggdb")

set(SOURCE_FILES
        src/parser.h
        src/parser.c
        src/nodejs_http_parser/http_parser.h
        src/nodejs_http_parser/http_parser.c src/logger.h
        src/logger.c)

add_library(httpparser-c ${SOURCE_FILES})
set_property(TARGET httpparser-c PROPERTY POSITION_INDEPENDENT_CODE ON)

enable_testing()

include_directories(src)
link_libraries(httpparser-c z)
#add_executable(test_http_parse test/test_http_parser.c)
add_executable(test_decode test/test_decode.c)
add_executable(test_logger test/test_logger.c)
add_executable(test_parser_util test/test_parser_util.c)

#add_test(http_parse test_http_parse)
add_test(decode test_decode)
add_test(logger test_logger)
add_test(parser_util test_parser_util)
