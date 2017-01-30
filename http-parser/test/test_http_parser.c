#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "logger.h"
#include "parser.h"

#include "test_http_parser.h"

int callbacks_mask;
int content_length;

int http_request_received(connection_context *context, void *message) {
    callbacks_mask |= HTTP_REQUEST_RECEIVED;
    return 0;
}

int http_request_body_started(connection_context *context) {
    callbacks_mask |= HTTP_REQUEST_BODY_STARTED;
    content_length = 0;
    return 0;
}

void http_request_body_data(connection_context *context, const char *data, size_t length) {
    callbacks_mask |= HTTP_REQUEST_BODY_DATA;
    content_length += length;
}
void http_request_body_finished(connection_context *context) {
    callbacks_mask |= HTTP_REQUEST_BODY_FINISHED;
}
int http_response_received(connection_context *context, void *message) {
    callbacks_mask |= HTTP_RESPONSE_RECEIVED;
    return 0;
}

int http_response_body_started(connection_context *context) {
    callbacks_mask |= HTTP_RESPONSE_BODY_STARTED;
    return 0;
}

void http_response_body_data(connection_context *context, const char *data, size_t length) {
    callbacks_mask |= HTTP_RESPONSE_BODY_DATA;
    content_length += length;
}

void http_response_body_finished(connection_context *context) {
    callbacks_mask |= HTTP_RESPONSE_BODY_FINISHED;
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

int main(int argc, char **argv) {
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

    int count = sizeof(messages) / sizeof(struct test_message);
    for (int i = 0; i < count; i++) {
        struct test_message *message = &messages[i];
        fprintf(stderr, "Processing stream:\n%s\n", message->data);
        callbacks_mask = 0;
        content_length = 0;
        assert(parser_input(cctx, message->direction, message->data, strlen(message->data)) == 0);
        assert(callbacks_mask == message->callbacks_mask);
        assert(content_length == message->content_length);
    }

    return 0;
}
