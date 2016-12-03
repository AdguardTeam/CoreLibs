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
#include "logger.h"

#include "../zlib/zlib.h"

#define PARSER_LOG(args...) logger_log(parser_ctx->log, args)
#define CTX_LOG(args...) logger_log(context->parser_ctx->log, args)

/**
 * Create HTTP message
 * @param message Pointer to variable where pointer to newly allocated message will be placed
 */
static inline void create_http_message(http_message **message) {
    *message = malloc(sizeof(http_message));
    memset(*message, 0, sizeof(http_message));
}

/**
 * Destroy HTTP message, including its state and field variables
 * @param message Pointer to HTTP message
 */
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

/**
 * External interface for destroy_http_message()
 * @param message Pointer to HTTP message
 */
void http_message_free(http_message *message) {
    destroy_http_message(message);
}

/**
 * Allocates place for next HTTP header parameter
 * @param message Pointer to HTTP message
 */
static void add_http_header_param(http_message *message) {
    message->field_count++;
    if (message->fields == NULL) {
        message->fields = malloc(sizeof(http_header_field));
        memset(message->fields, 0, sizeof(http_header_field));
    } else {
        message->fields = realloc(message->fields, message->field_count * sizeof(http_header_field));
        memset(&message->fields[message->field_count - 1], 0, sizeof(http_header_field));
    }
}

/**
 * Appends chars from character array `src' to null-terminated string `dst'
 * @param dst Pointer to null-terminated string (may be reallocated)
 * @param src Character array
 * @param len Length of character array
 */
static inline void append_chars(char **dst, const char *src, size_t len) {
    size_t old_len;
    if (*dst == NULL) {
        *dst = malloc(len + 1);
        old_len = 0;
    } else {
        old_len = strlen(*dst);
        *dst = realloc(*dst, old_len + len + 1);
    }
    memcpy(*dst + old_len, src, len);
    (*dst)[old_len + len] = 0;
}

/**
 * Appends bytes from character array `src' to character array `dst'
 * `dst' may contain null bytes.
 * @param dst Pointer to character array (may be reallocated)
 * @param dst_len Pointer to variable that contains length on `dst' array
 * @param src Character array
 * @param len Length of character array
 */
static inline void append_bytes(char **dst, size_t *dst_len, const char *src, size_t len) {
    if (*dst == NULL) {
        *dst_len = 0;
    }
    *dst = realloc(*dst, *dst_len + len + 1);
    memcpy(*dst + *dst_len, src, len);
    (*dst)[*dst_len + len] = 0;
    *dst_len += len;
}

/**
 * Copy characters from `src' characted array to dst null-terminated string
 * @param dst Pointer to null-terminated string (may be reallocated)
 * @param src Character array
 * @param len Length of character array
 */
static inline void set_chars(char **dst, const char *src, size_t len) {
    *dst = realloc(*dst, len + 1);
    memcpy(*dst, src, len);
    (*dst)[len] = 0;
}

/**
 * Content-Encoding enum type.
 * Encoding supported by this library - `identity', `deflate' and `gzip'
 */
typedef enum {
    CONTENT_ENCODING_IDENTITY = 0,
    CONTENT_ENCODING_DEFLATE = 1,
    CONTENT_ENCODING_GZIP = 2
} content_encoding_t;

typedef struct parser_context parser_context;

/*
 * Connection context structure
 */
struct connection_context { /* structure has name for queue.h macros */
    // Connection id
    connection_id_t         id;
    // Current error message
    char                    error_message[256];
    // Body callback error
    error_type_t            body_callback_error;
    // Connection info (endpoint names), not used by any current language bindings
    connection_info         *info;
    // Parser callbacks
    parser_callbacks        *callbacks;
    // Pointer to Node.js http_parser implementation
    http_parser             *parser;
    // Pointer to parser settings
    http_parser_settings    *settings;
    // Pointer to message which is currently being constructed
    http_message            *message;
    // Pointer to parent context
    parser_context          *parser_ctx;

    // State flags:
    // Message construction is done
    size_t                  done;
    // We are currently in field (after retrieveing field name and before reteiving field value)
    int                     in_field;
    /* Flag if message have body (Content-length is more than zero, body can yet be empty if
     * consists to one empty chunk)
     */
    int                     have_body;
    // We are currently in body and body decoding started (if message body needs any kind of decoding)
    int                     body_started;
    // Decode is needed flag
    int                     need_decode;
    // Content-Encoding of body - identity (no encoding), deflate, gzip. Determined from headers.
    content_encoding_t      content_encoding;
    // Zlib decode input buffer. Contains tail on previous input buffer which can't be processed right now
    char                    *decode_in_buffer;
    // Zlib decode output buffer. Contains currently decompressed data
    char                    *decode_out_buffer;
    // Zlib stream
    z_stream                zlib_stream;

    // Content-by-id hashmap entry (queue.h based)
    // TODO: may be it is better to use klib/khash instead
    TAILQ_ENTRY(connection_context) context_by_id_entry;
};

/*
 * Functions for body decompression.
 * Since decompression of one chunk of data may result on more than one output chunk, passing callback is needed.
 */
/*
 * Body data callback. Usually it is parser_callbacks.http_request_body_data()
 */
typedef void (*body_data_callback)(connection_context *context, const char *at, size_t length);
/*
 * Initializes zlib stream for current HTTP message body.
 * Used internally by http_parser_on_body().
 */
static int message_inflate_init(connection_context *context);
/*
 * Inflate current input buffer and call body_data_callback for each decompressed chunk
 */
static int message_inflate(connection_context *context, const char *data, size_t length, body_data_callback body_data);
/*
 * Deinitializes zlib stream for current HTTP message body.
 */
static int message_inflate_end(connection_context *context);

/*
 * Other utility functions.
 */
/**
 * Throw an error for current connection context. It calls parse_error callback with given error type and message
 * @param context Connection context
 * @param direction Transfer direction
 * @param msg Error message (may be null or empty)
 */
static void set_error(connection_context *context, const char *msg);

/*
 *  Node.js http_parser's callbacks (parser->settings):
 */
/* For in-callback using only! */

static content_encoding_t get_content_encoding(connection_context *context);

void parser_reset(connection_context *context);

#define CONTEXT(parser)         ((connection_context*)parser->data)

/**
 * Context by id hash map implementation
 */
#define HASH_SIZE 63

TAILQ_HEAD(context_by_id, connection_context);
struct parser_context {
    struct context_by_id context_by_id_hash[HASH_SIZE];
    int context_by_id_hash_initialized;
    logger *log;
};

static void context_by_id_init(parser_context *parser_ctx) {
    for (int i = 0; i < HASH_SIZE; i++) {
        TAILQ_INIT(&parser_ctx->context_by_id_hash[i]);
    }
}

static connection_context *context_by_id_get(parser_context *parser_ctx, connection_id_t id) {
    if (!parser_ctx->context_by_id_hash_initialized) {
        context_by_id_init(parser_ctx);
        parser_ctx->context_by_id_hash_initialized = 1;
    }
    struct connection_context *context;
    TAILQ_FOREACH(context, &parser_ctx->context_by_id_hash[id % HASH_SIZE], context_by_id_entry) {
        if (context->id == id) {
            return context;
        }
    }
    return NULL;
}

static void context_by_id_add(parser_context *parser_ctx, connection_context *context) {
    context->parser_ctx = parser_ctx;
    TAILQ_INSERT_HEAD(&parser_ctx->context_by_id_hash[context->id % HASH_SIZE], context, context_by_id_entry);
}

static connection_context *context_by_id_remove(parser_context *parser_ctx, connection_id_t id) {
    struct connection_context *context;
    TAILQ_FOREACH(context, &parser_ctx->context_by_id_hash[id % HASH_SIZE], context_by_id_entry) {
        if (context->id == id) {
            TAILQ_REMOVE(&parser_ctx->context_by_id_hash[id % HASH_SIZE], context, context_by_id_entry);
            return context;
        }
    }
    return NULL;
}

/*
 *  Internal callbacks:
 */
int http_parser_on_message_begin(http_parser *parser) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_begin(parser=%p)", parser);
    create_http_message(&context->message);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_begin() returned %d", 0);
    return 0;
}

int http_parser_on_url(http_parser *parser, const char *at, size_t length) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_url(parser=%p, at=%.*s)", parser, (int) length, at);
    http_message *message = context->message;
    if (at != NULL && length > 0) {
        append_chars(&message->url, at, length);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_url() returned %d", 0);
    return 0;
}

int http_parser_on_status(http_parser *parser, const char *at, size_t length) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_status(parser=%p, at=%.*s)", parser, (int) length, at);
    http_message *message = context->message;
    if (at != NULL && length > 0) {
        append_chars(&message->status, at, length);
        message->status_code = parser->status_code;
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_status() returned %d", 0);
    return 0;
}

int http_parser_on_header_field(http_parser *parser, const char *at, size_t length) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_field(parser=%p, at=%.*s)", parser, (int) length, at);
    http_message *message = context->message;
    if (at != NULL && length > 0) {
        if (!context->in_field) {
            context->in_field = 1;
            add_http_header_param(message);
        }
        append_chars(&message->fields[message->field_count - 1].name, at, length);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_field() returned %d", 0);
    return 0;
}

int http_parser_on_header_value(http_parser *parser, const char *at, size_t length) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_value(parser=%p, at=%.*s)", parser, (int) length, at);
    http_message *message = context->message;
    context->in_field = 0;
    if (at != NULL && length > 0) {
        append_chars(&message->fields[message->field_count - 1].value, at, length);
    } else {
        message->fields[message->field_count - 1].value = calloc(1, 1);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_value() returned %d", 0);
    return 0;
}

int http_parser_on_headers_complete(http_parser *parser) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_headers_complete(parser=%p)", parser);
    http_message *message = context->message;
    const char *method;
    int skip = 0;
    switch (parser->type) {
        case HTTP_REQUEST:
            method = http_method_str(parser->method);
            set_chars(&message->method, method, strlen(method));
            skip = context->callbacks->http_request_received(context, message);
            break;
        case HTTP_RESPONSE:
            skip = context->callbacks->http_response_received(context, message);
            break;
        default:
            break;
    }

    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_headers_complete() returned %d", skip);
    return skip;
}

int http_parser_on_body(http_parser *parser, const char *at, size_t length) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_body(parser=%p, length=%d)", parser, (int) length);
    context->have_body = 1;
    int (*body_started)(connection_context *);
    body_data_callback body_data;
    transfer_direction_t direction;
    error_type_t r = PARSER_OK;
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
            r = PARSER_INVALID_ARGUMENT_ERROR;
            goto out;
    }

    if (context->body_started == 0) {
        context->need_decode = body_started(context);
        if (context->need_decode) {
            if (message_inflate_init(context) != 0) {
                set_error(context, context->zlib_stream.msg);
                r = PARSER_ZLIB_ERROR;
                goto out;
            }
        }
        context->body_started = 1;
    }
    if (context->content_encoding == CONTENT_ENCODING_IDENTITY) {
        body_data(context, at, length);
    } else {
        if (message_inflate(context, at, length, body_data) != 0) {
            set_error(context, context->zlib_stream.msg);
            return PARSER_ZLIB_ERROR;
        }
    }

    out:
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_body() returned %d", r);
    context->body_callback_error = r;
    return r;
}

static void set_error(connection_context *context, const char *msg) {
    snprintf(context->error_message, 256, "%s", msg ? msg : "");
}

/**
 * Initialize Zlib stream depending on Content-Encoding (gzip/deflate)
 * @param context Connection context
 */
static int message_inflate_init(connection_context *context) {
    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate_init()");
    context->content_encoding = get_content_encoding(context);
    if (!context->need_decode || context->content_encoding == CONTENT_ENCODING_IDENTITY) {
        // Uncompressed
        context->decode_in_buffer = NULL;
        context->decode_out_buffer = NULL;
        return 0;
    }

    memset(&context->zlib_stream, 0, sizeof(z_stream));
    context->decode_in_buffer = malloc(ZLIB_DECOMPRESS_CHUNK_SIZE);
    memset(context->decode_in_buffer, 0, ZLIB_DECOMPRESS_CHUNK_SIZE);
    context->decode_out_buffer = malloc(ZLIB_DECOMPRESS_CHUNK_SIZE);
    memset(context->decode_out_buffer, 0, ZLIB_DECOMPRESS_CHUNK_SIZE);

    int r;
    switch (context->content_encoding) {
        case CONTENT_ENCODING_DEFLATE:
            r = inflateInit(&context->zlib_stream);
            break;
        case CONTENT_ENCODING_GZIP:
            r = inflateInit2(&context->zlib_stream, 16 + MAX_WBITS);
            break;
        default:
            r = 0;
            break;
    }

    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate_init() returned %d", r);
    return r;
}

static content_encoding_t get_content_encoding(connection_context *context) {
    http_message *message = context->message;
    char *field_name = "Content-Encoding";
    size_t value_length;
    const char *value = http_message_get_header_field(message, field_name, strlen(field_name), &value_length);
    if (!strncasecmp(value, "gzip", value_length) || !strncasecmp(value, "x-gzip", value_length)) {
        return CONTENT_ENCODING_GZIP;
    } else if (!strncasecmp(value, "deflate", value_length)) {
        return CONTENT_ENCODING_DEFLATE;
    } else {
        return CONTENT_ENCODING_IDENTITY;
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
    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate(data=%p, length=%d)", data, (int) length);
    int result;
    int r = 0;

    if (context->decode_out_buffer == NULL) {
        r = 1;
        // Compression stream was not initialized
        goto finish;
    }

    // If we have a data in input buffer, append new data to it, otherwise process `data' as input buffer
    if (context->zlib_stream.avail_in > 0) {
        context->zlib_stream.next_in = (Bytef *) context->decode_in_buffer;
        if (context->zlib_stream.avail_in + length > ZLIB_DECOMPRESS_CHUNK_SIZE) {
            result = Z_BUF_ERROR;
            goto error;
        }
        memcpy(context->decode_in_buffer + context->zlib_stream.avail_in, data, length);
        context->zlib_stream.avail_in += (uInt) length;
    } else {
        context->zlib_stream.next_in = (Bytef *) data;
        context->zlib_stream.avail_in = (uInt) length;
    }

    int old_avail_in; // Check if we made a progress
    do {
        old_avail_in = context->zlib_stream.avail_in;
        context->zlib_stream.avail_out = ZLIB_DECOMPRESS_CHUNK_SIZE;
        context->zlib_stream.next_out = (Bytef *) context->decode_out_buffer;
        result = inflate(&context->zlib_stream, 0);
        if (result == Z_OK || result == Z_STREAM_END) {
            size_t processed = ZLIB_DECOMPRESS_CHUNK_SIZE - context->zlib_stream.avail_out;
            if (processed > 0) {
                // Call callback function for each decompressed block
                body_data(context, context->decode_out_buffer, processed);
            }
        }
        if (result != Z_OK) {
            goto error;
        }
    } while (context->zlib_stream.avail_in > 0 && old_avail_in != context->zlib_stream.avail_in);

    // Move unprocessed tail to the start of input buffer
    if (context->zlib_stream.avail_in) {
        memcpy(context->decode_in_buffer, context->zlib_stream.next_in, context->zlib_stream.avail_in);
    }

    goto finish;

    error:
    message_inflate_end(context); /* result ignored */
    if (result != Z_STREAM_END) {
        fprintf(stderr, "Decompression error: %d\n", result);
        r = 1;
    }

finish:
    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate() returned %d", r);
    return r;
}

/**
 * Deinitialize stream compression structures
 * @param context
 */
static int message_inflate_end(connection_context *context) {
    int result = inflateEnd(&context->zlib_stream);
    free(context->decode_in_buffer);
    context->decode_in_buffer = NULL;
    free(context->decode_out_buffer);
    context->decode_out_buffer = NULL;
    return result;
}

int http_parser_on_message_complete(http_parser *parser) {
    connection_context *context = CONTEXT(parser);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_complete(parser=%p)", parser);
    http_message *message = context->message;
    if (context->have_body) {
        switch (parser->type) {
            case HTTP_REQUEST:
                context->callbacks->http_request_body_finished(context);
                break;
            case HTTP_RESPONSE:
                context->callbacks->http_response_body_finished(context);
                break;
            default:
                break;
        }
    }

    parser_reset(context);

    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_complete() returned %d", 0);
    return 0;
}

int http_parser_on_chunk_header(http_parser *parser) {
    // ignore
    return 0;
}

int http_parser_on_chunk_complete(http_parser *parser) {
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
 *  API implementation
 */

int parser_create(logger *log, parser_context **p_parser_ctx) {
    if (p_parser_ctx == NULL) {
        return PARSER_NULL_POINTER_ERROR;
    }

    *p_parser_ctx = calloc(1, sizeof(parser_context));
    (*p_parser_ctx)->log = log;

    logger_log(log, LOG_LEVEL_TRACE, "parser_create()");

    return 0;
}

int parser_destroy(parser_context *parser_ctx) {
    PARSER_LOG(LOG_LEVEL_TRACE, "parser_destroy()");
    if (parser_ctx->context_by_id_hash_initialized) {
        for (int i = 0; i < HASH_SIZE; i++) {
            struct connection_context *context;
            TAILQ_FOREACH(context, &parser_ctx->context_by_id_hash[i], context_by_id_entry) {
                parser_connection_close(context);
            }
        }
    }
    PARSER_LOG(LOG_LEVEL_TRACE, "parser_destroy() finished.");
}

int parser_connect(parser_context *parser_ctx, connection_id_t id, parser_callbacks *callbacks, connection_context **p_context) {
    PARSER_LOG(LOG_LEVEL_TRACE, "parser_connect(id=%d, callbacks=%p, p_context=%p)", (int)id, callbacks, p_context);
    connection_context *context = context_by_id_get(parser_ctx, id);
    int r = PARSER_OK;

    if (context != NULL) {
        // Already connected
        PARSER_LOG(LOG_LEVEL_TRACE, "error: already connected!");
        set_error(context, "Already connected");
        r = PARSER_ALREADY_CONNECTED_ERROR;
        goto finish;
    }
    
    context = malloc(sizeof(connection_context));
    memset(context, 0, sizeof(connection_context));

    context->id = id;
    context->callbacks = callbacks;

    context->settings = &_settings;
    context->parser = malloc(sizeof(http_parser));

    context->parser->data = context;
    
    context_by_id_add(parser_ctx, context);
    parser_reset(context);

    if (p_context != NULL) {
        PARSER_LOG(LOG_LEVEL_TRACE, "setting *p_context to %p", context);
        *p_context = context;
    } else {
        r = PARSER_NULL_POINTER_ERROR;
        set_error(context, "p_context is NULL");
    }

    finish:
    PARSER_LOG(LOG_LEVEL_TRACE, "parser_connect() returned %d", r);
    return r;
}

void parser_reset(connection_context *context) {
    if (context->decode_out_buffer != NULL) {
        message_inflate_end(context);
    }

    if (context->message != NULL) {
        destroy_http_message(context->message);
    }
    context->message = 0;
    context->have_body = 0;
    context->body_started = 0;
    context->content_encoding = CONTENT_ENCODING_IDENTITY;

    /* Re-init parser before next message. */
    http_parser_init(context->parser, HTTP_BOTH);
}

int parser_disconnect(connection_context *context, transfer_direction_t direction) {
    CTX_LOG(LOG_LEVEL_TRACE, "parser_disconnect(context=%p, direction=%d)", context, (int) direction);
    if (direction == DIRECTION_OUT) {
        if (context->parser->type == HTTP_RESPONSE) {
            message_inflate_end(context);
            http_parser_init(context->parser, HTTP_REQUEST);
        }
    }
    CTX_LOG(LOG_LEVEL_TRACE, "parser_disconnect() returned %d", 0);
    return 0;
}

#define INPUT_LENGTH_AT_ERROR 1

int parser_input(connection_context *context, transfer_direction_t direction, const char *data,
          size_t length) {
    CTX_LOG(LOG_LEVEL_TRACE, "parser_input(context=%p, direction=%d, len=%d)", context, (int) direction, (int) length);
    // TODO: this is wrong, null bytes are allowed in content
    context->done = 0;

    if (HTTP_PARSER_ERRNO(context->parser) != HPE_OK || context->parser->type == HTTP_BOTH) {
        http_parser_init(context->parser, direction == DIRECTION_OUT ? HTTP_REQUEST : HTTP_RESPONSE);
    }

    context->done = http_parser_execute(context->parser, context->settings,
                                         data, length);

    int r = 0;
    while (context->done < length)
    {
        if (HTTP_PARSER_ERRNO(context->parser) != HPE_OK) {
            enum http_errno http_parser_errno = HTTP_PARSER_ERRNO(context->parser);
            if (http_parser_errno != HPE_CB_body) {
                // If body data callback fails, then get saved error from structure, don't overwrite
                set_error(context, http_errno_description(http_parser_errno));
                r = PARSER_HTTP_PARSE_ERROR;
            } else {
                r = context->body_callback_error;
            }

            // http_parser_init(context->parser, direction == DIRECTION_OUT ? HTTP_REQUEST : HTTP_RESPONSE);
            goto finish;
        }

        // Input was not fully processed, turn on slow mode
        http_parser_execute(context->parser, context->settings,
                             data + context->done, INPUT_LENGTH_AT_ERROR);
        context->done+=INPUT_LENGTH_AT_ERROR;
    }

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "parser_input() returned %d", r);
    return r;
}

int parser_connection_close(connection_context *context) {
    context_by_id_remove(context->parser_ctx, context->id);
    free(context);
    return 0;
}

/*
 *  Utility methods definition:
 */

http_message *http_message_create() {
    return calloc(1, sizeof(http_message));
}

http_message *http_message_clone(const http_message *source) {
    http_message *message;
    create_http_message(&message);
    if (source->url != NULL)
        set_chars(&message->url, source->url, strlen(source->url));
    if (source->status != NULL)
        set_chars(&message->status, source->status, strlen(source->status));
    if (source->method != NULL)
        set_chars(&message->method, source->method, strlen(source->method));
    message->status_code = source->status_code;
    for (int i = 0; i < source->field_count; i++) {
        add_http_header_param(message);
        set_chars(&message->fields[i].name, source->fields[i].name,
                       strlen(source->fields[i].name));
        set_chars(&message->fields[i].value, source->fields[i].value,
                       strlen(source->fields[i].value));
    }
    return message;
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
            set_chars(&message->fields[i].value, value, value_length);
            return 0;
        }
    }
    return  1;
}

const char *http_message_get_header_field(const http_message *message, const char *name,
                                          size_t name_length, size_t *p_value_length) {
    if (message == NULL || name == NULL || name_length == 0) return NULL;
    for (int i = 0; i < message->field_count; i++) {
        if (strncmp(message->fields[i].name, name, name_length) == 0) {
            *p_value_length = strlen(message->fields[i].value);
            return message->fields[i].value;
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
    message->fields[message->field_count - 1].value = calloc(1, 1);
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
                message->fields[j - 1].name =
                    message->fields[j].name;
                message->fields[j - 1].value =
                    message->fields[j].value;
            }
            message->field_count--;
            message->fields = realloc(message->fields, message->field_count * sizeof(http_header_field));
            return 0;
        }
    }
    return 1;
}

char *http_message_raw(const http_message *message, size_t *p_length) {
    char *out_buffer;
    size_t length, line_length = 0;

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

    if (p_length != NULL) {
        *p_length = length;
    }
    return out_buffer;
}

connection_id_t connection_get_id(connection_context *context) {
    return context->id;
}

const char *connection_get_error_message(connection_context *context) {
    return context->error_message;
}

