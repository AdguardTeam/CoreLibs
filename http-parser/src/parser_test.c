/*
 *  HTTP parser test.
 */
#include "parser.h"
#include "test_streams.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define INPUT_PORTION 1

http_message *saved_messages[100];
unsigned int num_messages = 0;

/*
 *  Helpers:
 */
#define DBG_CALLBACK

#ifndef DBG_CALLBACK
#define DBG_CALLBACK \
     printf ("CALLBACK [%s]:\n", __FUNCTION__);
#endif

void print_header(http_message *header) {
    if (!header) return;
    printf("\n");
    if (header->method) printf("METHOD: %s\n", header->method);
    if (header->url) printf("URL: %s\n", header->url);
    if (header->status) printf("STATUS: %s\n", header->status);
    if (header->status_code) printf("STATUS CODE: %d\n", header->status_code);
    for (int i = 0; i < header->field_count; i++) {
        printf ("%s: %s\n", header->fields[i].name, header->fields[i].value);
    }
    return;
}

void print_message(http_message *message) {
    if (!message) return;
    print_header(message->header);
    if (message->body) printf("BODY:%s\n", message->body);
    return;
}

/*
 *  User-defined callback for testing;
 */
int request_received(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    //print_header((http_message *)((http_message *)data)->header);
    return 0;
}

int request_body_started(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    return 0;
}

int request_body_data(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    return 0;
}

int request_body_finished(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    saved_messages[num_messages]=
        http_message_clone((http_message *)data);
    num_messages++;
    //printf ("BODY:\n%s\n", (http_message *)((http_message *)data)->body);
    return 0;
}

int response_received(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    //print_header((http_message *)((http_message *)data)->header);
    return 0;
}

int response_body_started(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    return 0;
}

int response_body_data(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    return 0;
}

int response_body_finished(connection_id id, void *data, size_t length) {
    DBG_CALLBACK
    saved_messages[num_messages]=
        http_message_clone((http_message *)data);
    num_messages++;
    //printf ("BODY:\n%s\n", (http_message *)((http_message *)data)->body);
    return 0;
}


parser_callbacks test_callbacks = {
    request_received,
    request_body_started,
    request_body_data,
    request_body_finished,
    response_received,
    response_body_started,
    response_body_data,
    response_body_finished
};


int main(int argc, char **argv) {
    int pos = 0, length = strlen (test_stream);
    parser_callbacks *callbacks = &test_callbacks; 

    parser_connect (1, NULL, callbacks);

    do {
        parser_input (1, 1, test_stream + pos, INPUT_PORTION);
    } while ((pos += INPUT_PORTION) < length);

    for (int i = 0; i < num_messages; i++) {
        printf("%s", http_message_raw(saved_messages[i]));
        //print_message(saved_messages[i]);
    }
    return 0;
}
