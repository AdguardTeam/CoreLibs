#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <fcntl.h>
#include "../src/logger.h"
#include "../src/parser.h"

#define DECODE 1

const char *current_file;
char *outbuf;
int outpos;
int finished;

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
    memcpy(outbuf + outpos, data, length);
    outpos += length;
}

void http_response_body_finished(connection_context *context) {
    fputc(']', stderr);
    finished = 1;
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

char *license_txt;
int license_txt_len;
char *license_txt_http_gzip;
int license_txt_http_gzip_len;
char *license_txt_http_gzip_chunked;
int license_txt_http_gzip_chunked_len;

void prepare(char *file_name, char **buf, int *len) {
    fprintf(stderr, "Reading %s... ", file_name);
    FILE *file = fopen(file_name, "r");
    assert (file != NULL);
    fseek(file, 0L, SEEK_END);
    *len = (int) ftell(file);
    fseek(file, 0L, SEEK_SET);
    *buf = malloc(*len);
    char *pos = *buf;
    int r;
    while ((r = fread(pos, 1, 4096, file)) > 0) {
        pos += r;
    }
    assert ((pos - *buf) == *len);
    fprintf(stderr, "%d bytes read.\n", *len);
}

void process(connection_context *cctx, char *file_name, char *buf, int len, char *uncompressed_buf, int uncompressed_len) {
    fprintf(stderr, "Processing %s: ", file_name);
    outbuf = malloc(uncompressed_len);
    outpos = 0;
    finished = 0;
    int r;
    r = parser_input(cctx, DIRECTION_IN, buf, len);
    fputc('\n', stderr);
    if (r != 0) {
        fprintf(stderr, "parser_input() returned non-zero status: %d\n", r);
        exit(1);
    }
    // input is fully processed
    assert (finished);
    assert (outpos == uncompressed_len);
    assert (!memcmp(outbuf, uncompressed_buf, uncompressed_len));
}

int main(int argc, char **argv) {
    prepare("data/LICENSE-2.0.txt", &license_txt, &license_txt_len);
    prepare("data/LICENSE-2.0.txt-HTTP-gzip.bin", &license_txt_http_gzip, &license_txt_http_gzip_len);
    prepare("data/LICENSE-2.0.txt-HTTP-gzip-chunked.bin", &license_txt_http_gzip_chunked, &license_txt_http_gzip_chunked_len);

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

    process(cctx, "data/LICENSE-2.0.txt-HTTP-gzip.bin",
            license_txt_http_gzip, license_txt_http_gzip_len,
            license_txt, license_txt_len);
    process(cctx, "data/LICENSE-2.0.txt-HTTP-gzip-chunked.bin",
            license_txt_http_gzip_chunked, license_txt_http_gzip_chunked_len,
            license_txt, license_txt_len);
    return 0;
}
