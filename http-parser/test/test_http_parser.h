#ifndef TEST_STREAMS_H
#define TEST_STREAMS_H

#include "parser.h"

#define HTTP_REQUEST_RECEIVED       (1 << 0)
#define HTTP_REQUEST_BODY_STARTED   (1 << 1)
#define HTTP_REQUEST_BODY_DATA      (1 << 2)
#define HTTP_REQUEST_BODY_FINISHED  (1 << 3)
#define HTTP_RESPONSE_RECEIVED      (1 << 4)
#define HTTP_RESPONSE_BODY_STARTED  (1 << 5)
#define HTTP_RESPONSE_BODY_DATA     (1 << 6)
#define HTTP_RESPONSE_BODY_FINISHED (1 << 7)

struct test_message {
    transfer_direction_t direction;
    int content_length;
    int callbacks_mask;
    char *data;
};

struct test_message messages[] = {
{ DIRECTION_OUT, 0, HTTP_REQUEST_RECEIVED,
        "GET /test HTTP/1.1\r\n"
        "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
        "Host: 0.0.0.0=5000\r\n"
        "Accept: */*\r\n"
        "\r\n"
},

{ DIRECTION_IN, 219, HTTP_RESPONSE_RECEIVED | HTTP_RESPONSE_BODY_STARTED | HTTP_RESPONSE_BODY_DATA | HTTP_RESPONSE_BODY_FINISHED,
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
},

{ DIRECTION_OUT, 0, HTTP_REQUEST_RECEIVED,
        "GET /test HTTP/1.1\r\n"
        "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
        "Host: 0.0.0.0=5000\r\n"
        "Accept: */*\r\n"
        "\r\n"
},

{ DIRECTION_OUT, 4, HTTP_REQUEST_RECEIVED | HTTP_REQUEST_BODY_STARTED | HTTP_REQUEST_BODY_DATA | HTTP_REQUEST_BODY_FINISHED,
        "POST / HTTP/1.1\r\n"
        "Host: www.example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "q=42\r\n"
},

{ DIRECTION_IN, 65, HTTP_RESPONSE_RECEIVED | HTTP_RESPONSE_BODY_STARTED | HTTP_RESPONSE_BODY_DATA | HTTP_RESPONSE_BODY_FINISHED,
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
},

{ DIRECTION_IN, 0, HTTP_RESPONSE_RECEIVED,
        "HTTP/1.1 500 ВАСИЛИЙ\r\n"
        "Date: Fri, 5 Nov 2010 23:07:12 GMT+2\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n"
},

{ DIRECTION_IN, 65, HTTP_RESPONSE_RECEIVED | HTTP_RESPONSE_BODY_STARTED | HTTP_RESPONSE_BODY_DATA | HTTP_RESPONSE_BODY_FINISHED,
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
},

{ DIRECTION_OUT, 4, HTTP_REQUEST_RECEIVED | HTTP_REQUEST_BODY_STARTED | HTTP_REQUEST_BODY_DATA | HTTP_REQUEST_BODY_FINISHED,
        "POST / HTTP/1.1\r\n"
        "Host: www.example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "q=42\r\n"
},

{ DIRECTION_IN, 1, HTTP_RESPONSE_RECEIVED | HTTP_RESPONSE_BODY_STARTED | HTTP_RESPONSE_BODY_DATA | HTTP_RESPONSE_BODY_FINISHED,
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
        "\r\n"
}

};

#endif /* TEST_STREAMS_H */
