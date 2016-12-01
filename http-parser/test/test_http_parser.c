const char *test_stream =
        "GET /test HTTP/1.1\r\n"
                "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
                "Host: 0.0.0.0=5000\r\n"
                "Accept: */*\r\n"
                "\r\n"
                "HTTP/1.1 301 Moved Permanently\r\n"
                "Location: http://www.google.com/\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Date: Sun, 26 Apr 2009 11:11:49 GMT\r\n"
                "Expires: Tue, 26 May 2009 11:11:49 GMT\r\n"
                "X-$PrototypeBI-Version: 1.6.0.3\r\n" /* $ char in header field */
                "Cache-Control: public, max-age=2592000\r\n"
                "Server: gws\r\n"
                "Content-Length:  219  \r\n"
                "\r\n"
                "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
                "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
                "<H1>301 Moved</H1>\n"
                "The document has moved\n"
                "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
                "</BODY></HTML>\r\n"
                "GET /test HTTP/1.1\r\n"
                "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
                "Host: 0.0.0.0=5000\r\n"
                "Accept: */*\r\n"
                "\r\n"
                "POST / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Content-Length: 4\r\n"
                "\r\n"
                "q=42\r\n"
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "25  \r\n"
                "This is the data in the first chunk\r\n"
                "\r\n"
                "1C\r\n"
                "and this is the second one\r\n"
                "\r\n"
                "0  \r\n"
                "\r\n"
                "HTTP/1.1 500 ВАСИЛИЙ\r\n"
                "Date: Fri, 5 Nov 2010 23:07:12 GMT+2\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n"
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "25  \r\n"
                "This is the data in the first chunk\r\n"
                "\r\n"
                "1C\r\n"
                "and this is the second one\r\n"
                "\r\n"
                "0  \r\n"
                "\r\n"
                "POST / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Content-Length: 4\r\n"
                "\r\n"
                "q=42\r\n"
                "HTTP/1.1 301 MovedPermanently\r\n"
                "Date: Wed, 15 May 2013 17:06:33 GMT\r\n"
                "Server: Server\r\n"
                "x-amz-id-1: 0GPHKXSJQ826RK7GZEB2\r\n"
                "p3p: policyref=\"http://www.amazon.com/w3c/p3p.xml\",CP=\"CAO DSP LAW CUR ADM IVAo IVDo CONo OTPo OUR DELi PUBi OTRi BUS PHY ONL UNI PUR FIN COM NAV INT DEM CNT STA HEA PRE LOC GOV OTC \"\r\n"
                "x-amz-id-2: STN69VZxIFSz9YJLbz1GDbxpbjG6Qjmmq5E3DxRhOUw+Et0p4hr7c/Q8qNcx4oAD\r\n"
                "Location: http://www.amazon.com/Dan-Brown/e/B000AP9DSU/ref=s9_pop_gw_al1?_encoding=UTF8&refinementId=618073011&pf_rd_m=ATVPDKIKX0DER&pf_rd_s=center-2&pf_rd_r=0SHYY5BZXN3KR20BNFAY&pf_rd_t=101&pf_rd_p=1263340922&pf_rd_i=507846\r\n"
                "Vary: Accept-Encoding,User-Agent\r\n"
                "Content-Type: text/html; charset=ISO-8859-1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "\n\r\n"
                "0\r\n"
                "\r\n";
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