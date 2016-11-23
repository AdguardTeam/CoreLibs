#!/bin/bash

[ -x bin/parser_test ] && rm bin/parser_test

gcc -o bin/parser_test \
	src/nodejs_http_parser/http_parser.h \
	src/nodejs_http_parser/http_parser.c \
	src/parser.h \
    src/parser.c \
    zlib/zlib.h \
    zlib/libz.a \
	src/parser_test.c

[ $? -eq 0 ] && bin/parser_test test/test.list
