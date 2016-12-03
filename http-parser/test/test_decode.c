#define _GNU_SOURCE

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "logger.h"
#include "parser.h"

#define DECODE 1

struct process_context {
    char *buf;
    size_t pos;
    int finished;
} process_context;

struct test_file {
    char *name;
    char *contents;
    size_t size;
};

int http_request_received(connection_context *context, void *message) {
    return 0;
}

int http_request_body_started(connection_context *context) {
    return 0;
}

void http_request_body_data(connection_context *context, const char *data, size_t length) {

}
void http_request_body_finished(connection_context *context) {

}
int http_response_received(connection_context *context, void *message) {
    return 0;
}

int http_response_body_started(connection_context *context) {
    fputc('[', stderr);
    return DECODE;
}

void http_response_body_data(connection_context *context, const char *data, size_t length) {
    fputc('.', stderr);
    memcpy(process_context.buf + process_context.pos, data, length);
    process_context.pos += length;
}

void http_response_body_finished(connection_context *context) {
    fputc(']', stderr);
    process_context.finished = 1;
}

parser_callbacks cbs = {
    .http_request_received = http_request_received,
    .http_request_body_started = http_request_body_started,
    .http_request_body_data = http_request_body_data,
    .http_request_body_finished = http_request_body_finished,
    .http_response_received = http_response_received,
    .http_response_body_started = http_response_body_started,
    .http_response_body_data = http_response_body_data,
    .http_response_body_finished = http_response_body_finished
};

struct test_file license_txt;
struct test_file license_txt_http_gzip;
struct test_file license_txt_http_gzip_chunked;

void prepare(char *file_name, struct test_file *test_file) {
    fprintf(stderr, "Reading %s... ", file_name);
    FILE *file = fopen(file_name, "r");
    assert (file != NULL);
    fseek(file, 0L, SEEK_END);
    test_file->size = (size_t) ftell(file);
    fseek(file, 0L, SEEK_SET);
    test_file->contents = malloc(test_file->size);
    char *pos = test_file->contents;
    size_t r;
    while ((r = fread(pos, 1, 4096, file)) > 0) {
        pos += r;
    }
    size_t total_read = pos - test_file->contents;
    fprintf(stderr, "%ld bytes read.\n", total_read);
    assert (total_read == test_file->size);
    asprintf(&test_file->name, "%s", file_name);
}

void process(connection_context *cctx, struct test_file *file, struct test_file *uncompressed_file) {
    fprintf(stderr, "Processing %s: ", file->name);
    process_context.buf = malloc(uncompressed_file->size);
    process_context.pos = 0;
    process_context.finished = 0;
    int r;
    r = parser_input(cctx, DIRECTION_IN, file->contents, file->size);
    fputc('\n', stderr);
    if (r != 0) {
        fprintf(stderr, "parser_input() returned non-zero status: %d\n", r);
        exit(1);
    }
    // Input is fully processed
    assert (process_context.finished);
    // Length is same
    assert (process_context.pos == uncompressed_file->size);
    // Contents match
    assert (!memcmp(process_context.buf, uncompressed_file->contents, uncompressed_file->size));
}

int main(int argc, char **argv) {
    prepare("data/LICENSE-2.0.txt", &license_txt);
    prepare("data/LICENSE-2.0.txt-HTTP-gzip.bin", &license_txt_http_gzip);
    prepare("data/LICENSE-2.0.txt-HTTP-gzip-chunked.bin", &license_txt_http_gzip_chunked);

    logger *log = logger_open(NULL, LOG_LEVEL_INFO, NULL, NULL);
    parser_context *pctx;
    if (parser_create(log, &pctx) != 0) {
        fprintf(stderr, "Error creating parser\n");
        return 1;
    }
    connection_context *cctx;
    if (parser_connect(pctx, 1L, &cbs, &cctx) != 0) {
        fprintf(stderr, "Error creating connection\n");
        return 1;
    }

    process(cctx, &license_txt_http_gzip, &license_txt);
    process(cctx, &license_txt_http_gzip_chunked, &license_txt);
    return 0;
}
