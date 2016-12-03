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

# Tests

enable_testing()
add_subdirectory(test)
