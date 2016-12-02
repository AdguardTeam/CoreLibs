cmake_minimum_required(VERSION 3.0)
project(http-parser)

find_package(Threads)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

set(SOURCE_FILES
        src/parser.h
        src/parser.c
        src/nodejs_http_parser/http_parser.h
        src/nodejs_http_parser/http_parser.c src/logger.h
        src/logger.c)

link_libraries(z pthread)
add_library(httpparser-c ${SOURCE_FILES})
set_property(TARGET httpparser-c PROPERTY POSITION_INDEPENDENT_CODE ON)

enable_testing()

include_directories(src)

# Tests

link_libraries(httpparser-c)

# Logger test
add_executable(test_logger test/test_logger.c)
add_test(logger test_logger)

# Parser utility methods test
add_executable(test_parser_util test/test_parser_util.c)
add_test(parser_util test_parser_util)

# HTTP parser test
add_executable(test_http_parser test/test_http_parser.h test/test_http_parser.c)
add_test(http_parser test_http_parser)

# Decode test
add_executable(test_decode test/test_decode.c)
file(COPY test/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(decode test_decode)


