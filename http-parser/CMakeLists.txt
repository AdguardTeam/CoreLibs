cmake_minimum_required(VERSION 3.0)
project(jni)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/parser.h
        src/parser.c
        src/nodejs_http_parser/http_parser.h
        src/nodejs_http_parser/http_parser.c
        )

add_library(httpparser-c ${SOURCE_FILES})
set_property(TARGET httpparser-c PROPERTY POSITION_INDEPENDENT_CODE ON)
