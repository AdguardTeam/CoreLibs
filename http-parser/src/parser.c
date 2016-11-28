#define _GNU_SOURCE
/*
 *  HTTP parser internals.
 *  Based on http parser API from Node.js project. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "nodejs_http_parser/http_parser.h"
#include "parser.h"

#include "../zlib/zlib.h"

/*
 *  Debug helpers:
 */
#define DEBUG 0

#if !DEBUG
#   define DBG_PARSER_ERROR
#   define DBG_HTTP_CALLBACK
#   define DBG_HTTP_CALLBACK_DATA
#   define DBG_PARSER_TYPE
#else
#   define DBG_PARSER_ERROR                                                 \
        fprintf (stderr, "DEBUG: Parser error: %s\n",                                \
                http_errno_name(parser->http_errno));

#   define DBG_HTTP_CALLBACK                                                \
        fprintf (stderr, "DEBUG: %s\n", __FUNCTION__);

#   define DBG_HTTP_CALLBACK_DATA                                           \
        fprintf (stderr, "DEBUG: %s: ", __FUNCTION__);                               \
        char *out = malloc (length + 1);                   \
        memset (out, 0, length + 1);                                        \
        memcpy (out, at, length);                                           \
        printf("%s\n", out);                                                \
        free(out); 

#   define DBG_PARSER_TYPE                                                  \
        fprintf (stderr, "DEBUG: Parser type: %d\n", parser->type);

#   define DBG_PARSER_METHOD                                                \
        fprintf (stderr, "DEBUG: Parser type: %d\n", parser->method);
#endif

#define DBG_LINE do { fprintf(stderr, __FUNCTION__ ":%d\n", __LINE__); } while(0)

/*
 *  Goods:
 */
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


static inline void create_http_message(http_message **message) {
    *message = malloc(sizeof(http_message));
    memset(*message, 0, sizeof(http_message));
}

static void destroy_http_message(http_message *message) {
    free(message->url);
    free(message->status);
    free(message->method);
    for (int i = 0; i < message->field_count; i++) {
        free(message->fields[i].name);
        free(message->fields[i].value);
    }
    free(message->fields);
    free(message);
}

void http_message_free(http_message *message) {
    destroy_http_message(message);
}

static void add_http_header_param(http_message *header) {
    header->field_count++;
    if (header->fields == NULL) {
        header->fields = malloc(sizeof(http_header_field));
        memset(header->fields, 0, sizeof(http_header_field));
    } else {
        header->fields = realloc(header->fields, header->field_count * sizeof(http_header_field));
        memset(&header->fields[header->field_count - 1], 0, sizeof(http_header_field));
    }
}

static inline void append_chars(char **dst, const char *src, size_t len) {
    size_t old_len;
    if (*dst == NULL) {
        *dst = malloc(len + 1);
        old_len = 0;
    } else {
        // TODO: this is not correct: store length since http body can contain null bytes.
        old_len = strlen(*dst);
        *dst = realloc(*dst, old_len + len + 1);
    }
    memcpy(*dst + old_len, src, len);
    (*dst)[old_len + len] = 0;
}

static inline void append_bytes(char **dst, size_t *dst_len, const char *src, size_t len) {
    if (*dst == NULL) {
        *dst_len = 0;
    }
    *dst = realloc(*dst, *dst_len + len + 1);
    memcpy(*dst + *dst_len, src, len);
    (*dst)[*dst_len + len] = 0;
    *dst_len += len;
}

static inline void set_chars(char **dst, const char *src, size_t len) {
    *dst = realloc(*dst, len + 1);
    memcpy(*dst, src, len);
    (*dst)[len] = 0;
}

typedef enum {
    CONTENT_ENCODING_IDENTITY = 0,
    CONTENT_ENCODING_DEFLATE = 1,
    CONTENT_ENCODING_GZIP = 2
} content_encoding;

/*
 *  Private stuff:
 */
typedef struct connection_context {
    connection_id_t           id;
    connection_info         *info;
    parser_callbacks        *callbacks;
    http_parser             *parser;
    http_parser_settings    *settings;
    http_message             *message;
    size_t                  done;
    unsigned int            in_field;
    unsigned int            have_body;
    unsigned int            body_started;
    unsigned int            content_encoding;
    char                    *decode_buffer;
    z_stream                zlib_stream;
    char                    error_message[256];
    TAILQ_ENTRY(connection_context) context_by_id_entry;
} connection_context;

typedef void (*body_data_callback)(connection_id_t, const char*, size_t);
static int message_inflate_init(connection_context *context, content_encoding encoding);
static int message_inflate(connection_context *context, const char *data, size_t length, body_data_callback body_data);
static int message_inflate_end(connection_context *context);
static void throw_error(connection_context *context, transfer_direction_t direction, error_type_t type, const char *msg);

/*
 *  Node.js http_parser's callbacks (parser->settings):
 */
/* For in-callback using only! */

#define CONTEXT(parser)         ((connection_context*)parser->data)

/**
 * Context by id hashmap
 */
#define HASH_SIZE 63

static TAILQ_HEAD(context_by_id, connection_context) context_by_id_hash[HASH_SIZE];

static int context_by_id_hash_initialized = 0;

static void context_by_id_init() {
    for (int i = 0; i < HASH_SIZE; i++) {
        TAILQ_INIT(&context_by_id_hash[i]);
    }
}

static connection_context *context_by_id_get(connection_id_t id) {
    if (!context_by_id_hash_initialized) {
        context_by_id_init();
        context_by_id_hash_initialized = 1;
    }
    struct connection_context *context;
    TAILQ_FOREACH(context, &context_by_id_hash[id % HASH_SIZE], context_by_id_entry) {
        if (context->id == id) {
            return context;
        }
    }
    return NULL;
}

static void context_by_id_add(connection_context *context) {
    TAILQ_INSERT_HEAD(&context_by_id_hash[context->id % HASH_SIZE], context, context_by_id_entry);
}

static connection_context *context_by_id_remove(connection_id_t id) {
    struct connection_context *context;
    TAILQ_FOREACH(context, &context_by_id_hash[id % HASH_SIZE], context_by_id_entry) {
        if (context->id == id) {
            TAILQ_REMOVE(&context_by_id_hash[id % HASH_SIZE], context, context_by_id_entry);
            return context;
        }
    }
    return NULL;
}

/*
 *  Internal callbacks:
 */
int http_parser_on_message_begin(http_parser *parser) {
    DBG_HTTP_CALLBACK
    connection_context *context = CONTEXT(parser);
    create_http_message(&context->message);
    return 0;
}

int http_parser_on_url(http_parser *parser, const char *at, size_t length) {
    DBG_HTTP_CALLBACK_DATA
    connection_context *context = CONTEXT(parser);
    http_message *header = context->message;
    if (at != NULL && length > 0) {
        append_chars(&header->url, at, length);
    }
    return 0;
}

int http_parser_on_status(http_parser *parser, const char *at, size_t length) {
    DBG_HTTP_CALLBACK_DATA
    connection_context *context = CONTEXT(parser);
    http_message *header = context->message;
    if (at != NULL && length > 0) {
        append_chars(&header->status, at, length);
        header->status_code = parser->status_code;
    }
    return 0;
}

int http_parser_on_header_field(http_parser *parser, const char *at, size_t length) {
    DBG_HTTP_CALLBACK
    connection_context *context = CONTEXT(parser);
    http_message *header = context->message;
    if (at != NULL && length > 0) {
        if (!context->in_field) {
            context->in_field = 1;
            add_http_header_param(header);
        }
        append_chars(&header->fields[header->field_count - 1].name, at, length);
    }
    return 0;
}

int http_parser_on_header_value(http_parser *parser, const char *at, size_t length) {
    DBG_HTTP_CALLBACK
    connection_context *context = CONTEXT(parser);
    http_message *header = context->message;
    context->in_field = 0;
    if (at != NULL && length > 0) {
        append_chars(&header->fields[header->field_count - 1].value, at, length);
    } else {
        header->fields[header->field_count - 1].value = calloc(1, 1);
    }
    return 0;
}

int http_parser_on_headers_complete(http_parser *parser) {
    DBG_HTTP_CALLBACK
    connection_context *context = CONTEXT(parser);
    http_message *header = context->message;
    int skip = 0;
    switch (parser->type) {
        case HTTP_REQUEST:
            asprintf(&header->method, "%s", http_method_str(parser->method));
            skip = context->callbacks->http_request_received(context->id, header);
            break;
        case HTTP_RESPONSE:
            skip = context->callbacks->http_response_received(context->id, header);
            break;
        default:
            break;
    }

    return skip;
}

int http_parser_on_body(http_parser *parser, const char *at, size_t length) {
    DBG_HTTP_CALLBACK_DATA
    connection_context *context = CONTEXT(parser);
    context->have_body = 1;
    int (*body_started)(connection_id_t);
    body_data_callback body_data;
    transfer_direction_t direction;
    switch (parser->type) {
        case HTTP_REQUEST:
            body_started = context->callbacks->http_request_body_started;
            body_data = context->callbacks->http_request_body_data;
            direction = DIRECTION_OUT;
            break;
        case HTTP_RESPONSE:
            body_started = context->callbacks->http_response_body_started;
            body_data = context->callbacks->http_response_body_data;
            direction = DIRECTION_IN;
            break;
        default:
            return -1;
    }

    if (context->body_started == 0) {
        content_encoding encoding = (content_encoding) body_started(context->id);
        if (message_inflate_init(context, encoding) != 0) {
            throw_error(context, direction, PARSER_DECODE_ERROR, context->zlib_stream.msg);
            return 1;
        }
        context->body_started = 1;
    }
    if (context->content_encoding == CONTENT_ENCODING_IDENTITY) {
        body_data(context->id, at, length);
    } else {
        if (message_inflate(context, at, length, body_data) != 0) {
            throw_error(context, direction, PARSER_DECODE_ERROR, context->zlib_stream.msg);
            return 1;
        }
    }

    return 0;
}

static void throw_error(connection_context *context, transfer_direction_t direction, error_type_t type, const char *msg) {
    char error_message[256];
    snprintf(context->error_message, 256, "%s", msg);
    context->callbacks->parse_error(context->id, direction, type, error_message);
}

/**
 * Initialize Zlib stream depending on Content-Encoding (gzip/deflate)
 * @param context Connection context
 * @param encoding Content encoding
 */
static int message_inflate_init(connection_context *context, content_encoding encoding) {
    context->content_encoding = encoding;
    if (encoding == CONTENT_ENCODING_IDENTITY) {
        // Uncompressed
        context->decode_buffer = NULL;
        return 0;
    }

    memset(&context->zlib_stream, 0, sizeof(z_stream));
    context->decode_buffer = malloc(ZLIB_DECOMPRESS_CHUNK_SIZE);
    memset(context->decode_buffer, 0, ZLIB_DECOMPRESS_CHUNK_SIZE);

    switch (encoding) {
        case CONTENT_ENCODING_DEFLATE:
            return inflateInit(&context->zlib_stream);
        case CONTENT_ENCODING_GZIP:
            return inflateInit2(&context->zlib_stream, 16 + MAX_WBITS);
        default:
            return 0;
    }
}

/**
 * Decompress stream
 * @param context Connection context
 * @param data Input data
 * @param length Input data length
 * @param body_data Data callback function
 * @return 0 if data is successfully decompressed, 1 in case of error
 */
static int message_inflate(connection_context *context, const char *data, size_t length, body_data_callback body_data) {
    if (context->decode_buffer == NULL) {
        return 1;
    }

    context->zlib_stream.next_in = (Bytef *) data;
    context->zlib_stream.avail_in = (uInt) length;

    // Call callback function for each decompressed block
    do {
        context->zlib_stream.avail_out = ZLIB_DECOMPRESS_CHUNK_SIZE;
        context->zlib_stream.next_out = (Bytef *) context->decode_buffer;
        int result = inflate(&context->zlib_stream, 0);
        if (result == Z_OK || result == Z_STREAM_END) {
            size_t processed = ZLIB_DECOMPRESS_CHUNK_SIZE - context->zlib_stream.avail_out;
            if (processed > 0) {
                body_data(context->id, context->decode_buffer, processed);
            }
        }
        if (result != Z_OK) {
            message_inflate_end(context); /* result ignored */
            if (result != Z_STREAM_END) {
                fprintf(stderr, "Decompression error: %d\n", result);
                return -1;
            }
            goto finish;
        }
    } while (context->zlib_stream.avail_in > 0);

    finish:
    return 0;
}

/**
 * Deinitialize stream compression structures
 * @param context
 */
static int message_inflate_end(connection_context *context) {
    int result = inflateEnd(&context->zlib_stream);
    free(context->decode_buffer);
    context->decode_buffer = NULL;
    return result;
}

int http_parser_on_message_complete(http_parser *parser) {
    DBG_HTTP_CALLBACK
    connection_context *context = CONTEXT(parser);
    http_message *header = context->message;
    if (context->have_body) {
        switch (parser->type) {
            case HTTP_REQUEST:
                context->callbacks->http_request_body_finished(context->id);
                break;
            case HTTP_RESPONSE:
                context->callbacks->http_response_body_finished(context->id);
                break;
            default:
                break;
        }
    }

    if (context->decode_buffer != NULL) {
        message_inflate_end(context);
    }

    destroy_http_message(header);
    context->message = 0;
    context->have_body = 0;
    context->body_started = 0;
    context->content_encoding = 0;

    /* Re-init parser before next message. */
    http_parser_init(parser, HTTP_BOTH);

    return 0;
}

int http_parser_on_chunk_header(http_parser *parser) {
    DBG_HTTP_CALLBACK
    // ignore
    return 0;
}

int http_parser_on_chunk_complete(http_parser *parser) {
    DBG_HTTP_CALLBACK
    // ignore
    return 0;
}


http_parser_settings _settings = {
    .on_message_begin       = http_parser_on_message_begin,
    .on_url                 = http_parser_on_url,
    .on_status              = http_parser_on_status,
    .on_header_field        = http_parser_on_header_field,
    .on_header_value        = http_parser_on_header_value,
    .on_headers_complete    = http_parser_on_headers_complete,
    .on_body                = http_parser_on_body,
    .on_message_complete    = http_parser_on_message_complete,
    .on_chunk_header        = http_parser_on_chunk_header,
    .on_chunk_complete      = http_parser_on_chunk_complete
};

/*
 *  API implementaion:
 */
int parser_connect(connection_id_t id, connection_info *info,
            parser_callbacks *callbacks) {
    connection_context *context = context_by_id_get(id);
    if (context != NULL) {
        // Already connected
        // TODO: report error?
        return 1;
    }
    
    context = malloc(sizeof(connection_context));
    memset(context, 0, sizeof(connection_context));

    context->id = id;
    context->info = info;
    context->callbacks = callbacks;

    context->settings = &_settings;
    context->parser = malloc(sizeof(http_parser));

    context->parser->data = context;
    
    context_by_id_add(context);
    http_parser_init(context->parser, HTTP_BOTH);

    return 0;
}

int parser_disconnect(connection_id_t id, transfer_direction_t direction) {
    // connection_context *context = context_by_id_remove(id);
    // TODO: free context structures
    return 0;
}

#define INPUT_LENGTH_AT_ERROR 1

int parser_input(connection_id_t id, transfer_direction_t direction, const char *data,
          size_t length) {
    connection_context *context = context_by_id_get(id);
    if (context == NULL) {
        return 1;
    }

    // TODO: this is wrong, null bytes are allowed in content
    context->done = 0;

    if (HTTP_PARSER_ERRNO(context->parser) != HPE_OK) {
        http_parser_init(context->parser, HTTP_BOTH);
    }

    context->done = http_parser_execute(context->parser, context->settings,
                                         data, length);

    while (context->done < length)
    {
        if (HTTP_PARSER_ERRNO(context->parser) != HPE_OK) {
            throw_error(context, direction, PARSER_HTTP_ERROR, http_errno_name(HTTP_PARSER_ERRNO(context->parser)));
            http_parser_init(context->parser, HTTP_BOTH);
        }
        http_parser_execute(context->parser, context->settings,
                             data + context->done, INPUT_LENGTH_AT_ERROR);
        context->done+=INPUT_LENGTH_AT_ERROR;
    }

    return 0;
}

int parser_connection_close(connection_id_t id) {
    connection_context *context = context_by_id_remove(id);
    return 0;
}

/*
 *  Utility methods definition:
 */

http_message *http_message_clone(const http_message *source) {
    http_message *header;
    create_http_message(&header);
    if (source->url != NULL)
        set_chars(&header->url, source->url, strlen(source->url));
    if (source->status != NULL)
        set_chars(&header->status, source->status, strlen(source->status));
    if (source->method != NULL)
        set_chars(&header->method, source->method, strlen(source->method));
    header->status_code = source->status_code;
    for (int i = 0; i < source->field_count; i++) {
        add_http_header_param(header);
        set_chars(&header->fields[i].name, source->fields[i].name,
                       strlen(source->fields[i].name));
        set_chars(&header->fields[i].value, source->fields[i].value,
                       strlen(source->fields[i].value));
    }
    return header;
}

int http_message_set_method(http_message *message,
                            const char *method, size_t length) {
    if (method == NULL) return 1;
    if (message == NULL) return 1;
    if (message->method) {
        free(message->method);
        message->method = NULL;
    }
    set_chars(&message->method, method, length);
    return 0;
}

int http_message_set_url(http_message *message,
                         const char *url, size_t length) {
    if (url == NULL) return 1;
    if (message == NULL) return 1;
    if (message->url) {
        free(message->url);
        message->url = NULL;
    }
    set_chars(&message->url, url, length);
    return 0;
}

int http_message_set_status(http_message *message,
                            const char *status, size_t length) {
    if (status == NULL) return 1;
    if (message == NULL) return 1;
    if (message->status) free(message->status);
    set_chars(&message->status, status, length);
    return 0;
}

int http_message_set_status_code(http_message *message, int status_code) {
    if (message == NULL) return 1;
    message->status_code = status_code;
    return 0;
}

int http_message_set_header_field(http_message *message,
                                  const char *name, size_t name_length,
                                  const char *value, size_t value_length) {
    if (message == NULL || name == NULL || name_length == 0 ||
        value == NULL || value_length == 0 ) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncmp(message->fields[i].name, name, name_length) == 0) {
            if (message->fields[i].value != NULL) {
                free (message->fields[i].value);
                message->fields[i].value = NULL;
            }
            set_chars(&message->fields[i].value, value, value_length);
            return 0;
        }
    }
    return  1;
}

char *http_message_get_header_field(const http_message *message,
                                    const char *name, size_t length) {
    char *value;
    if (message == NULL || name == NULL || length == 0) return NULL;
    for (int i = 0; i < message->field_count; i++) {
        if (strncmp(message->fields[i].name, name, length) == 0) {
            set_chars(&value, message->fields[i].value,
                      strlen(message->fields[i].value));
            return value;
        }
    }
    return NULL;
}

int http_message_add_header_field(http_message *message,
                                  const char *name, size_t length) {
    if (message == NULL || name == NULL || length == 0) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncasecmp(message->fields[i].name, name, length) == 0)
            return 1;
    }
    add_http_header_param(message);
    append_chars(&message->fields[message->field_count - 1].name,
                 name, length);
    return 0;
}

int http_message_del_header_field(http_message *message,
                                  const char *name, size_t length) {
    if (message == NULL || name == NULL || length == 0) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncasecmp(message->fields[i].name, name, length) == 0) {
            free (message->fields[i].name);
            free (message->fields[i].value);
            for (int j = i + 1; j < message->field_count; j++) {
                message->fields[j -1 ].name =
                    message->fields[j].name;
                message->fields[j -1 ].value =
                    message->fields[j].value;
            }
            message->field_count--;
            message->fields = realloc(message->fields,
                                     message->field_count * sizeof(http_header_field));
            return 0;
        }
    }
    return 1;
}

char *http_message_raw(const http_message *message, size_t *out_length) {
    char *out_buffer;
    int length, line_length = 0;

    if (message == NULL) return NULL;

    if (message->status && message->status_code) {
        length = strlen(HTTP_VERSION) + strlen(message->status) + 7;
        out_buffer = malloc(length + 1);
        memset (out_buffer, 0, length + 1);
        snprintf(out_buffer, length + 1, "%s %u %s\r\n",
                 HTTP_VERSION, message->status_code,
                 message->status);
    } else {
        length = strlen(HTTP_VERSION) + strlen(message->url) +
                 strlen(message->method) + 4;
        out_buffer = malloc(length + 1);
        memset (out_buffer, 0, length + 1);
        snprintf(out_buffer, length + 1, "%s %s %s\r\n",
                 message->method, message->url, HTTP_VERSION);
    }

    for (int i = 0; i < message->field_count; i++) {
        line_length = strlen(message->fields[i].name) +
                      strlen(message->fields[i].value) + 4;
        out_buffer = realloc(out_buffer, length + line_length + 1);
        snprintf(out_buffer + length, line_length + 1, "%s: %s\r\n",
                 message->fields[i].name,
                 message->fields[i].value);
        length += line_length;
    }

    out_buffer = realloc(out_buffer, length + 3);
    snprintf(out_buffer + length, 3, "\r\n");
    length += 2;

    *out_length = length;
    return out_buffer;
}
