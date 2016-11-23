/*
 *  HTTP parser API.
 */
#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 *  Globals:
 */
#define HTTP_VERSION_1_1    "HTTP/1.1"
#define HTTP_VERSION        (HTTP_VERSION_1_1)

/*
 *  Types:
 */
typedef struct {
    char *name;
    char *value;
} http_header_field;

typedef struct {
    char                    *method;
    char                    *url;
    char                    *status;
    unsigned int            status_code;
    unsigned int            field_count;
    http_header_field      *fields;
} http_message;

typedef unsigned long connection_id;

/*  Connection is represented by two abstract endpoints, which are titled for
    easy identification by C-strings. */
#define ENDPOINT_MAXLEN 255

typedef struct {
    char endpoint_1[ENDPOINT_MAXLEN];
    char endpoint_2[ENDPOINT_MAXLEN];
} connection_info;

/**
 * Transfer direction
 * IN - Local client request
 * OUT - Remote server response
 */
enum transfer_direction {
    DIRECTION_IN                 = 0,
    DIRECTION_OUT                = 1
};
typedef enum transfer_direction transfer_direction;

typedef enum {
    PARSER_HTTP_ERROR = 0,
    PARSER_DECODE_ERROR
} error_type;

/**
 * Recommended chunk size for zlib inflate
 */
#define ZLIB_DECOMPRESS_CHUNK_SIZE 262144
#define ZLIB_COMPRESS_CHUNK_SIZE 65536

/*  User-defined callbacks.
    To inform parser about needed action, other then default "nothing to do", 
    callback should return non-zero code;
    In all other cases you should return 0. */
typedef int (*parser_cb)(connection_id id, void *data, size_t length);

typedef struct {
    /**
     * HTTP request received callback
     * @param id Connection id
     * @param header http_header structure
     * @return Non-null value if we are skipping this request
     */
    int (*http_request_received)(connection_id id, void *header);
    /**
     * HTTP request body started callback
     * @param id Connection id
     * @return Body compression type
     *          0 means uncompressed
     *          1 means zlib compression with deflate format
     *          2 means zlib compression with gzip format
     */
    int (*http_request_body_started)(connection_id id);
    /**
     * HTTP request body data callback
     * @param id Connection id
     * @param data Chunk of data (doesn't correspond to any HTTP chunk, just part of decoded stream)
     * @param length Length of data chunk
     */
    void (*http_request_body_data)(connection_id id, const char *data, size_t length);
    /**
     * HTTP request body finished callback
     * @param id Connection id
     */
    void (*http_request_body_finished)(connection_id id);
    /**
     * HTTP response received callback
     * @param id Connection id
     * @param header http_header structure
     * @return Non-null value if we are skipping this request/response
     */
    int (*http_response_received)(connection_id id, void *header);
    /**
     * HTTP response body started callback
     * @param id Connection id
     * @return Body compression type
     *          0 means uncompressed
     *          1 means zlib compression with deflate format
     *          2 means zlib compression with gzip format
     */
    int (*http_response_body_started)(connection_id id);
    /**
     * HTTP response body data callback
     * @param id Connection id
     * @param data Chunk of data (doesn't correspond to any real HTTP chunk, just part of decoded stream)
     * @param length Length of data chunk
     */
    void (*http_response_body_data)(connection_id id, const char *data, size_t length);
    /**
     * HTTP response body finished callback
     * @param id Connection id
     */
    void (*http_response_body_finished)(connection_id id);

    /**
     * HTTP parser error
     * @param id Connection id
     * @param type Error type (HTTP parser/decode)
     * @param message Error message
     */
    void (*parse_error)(connection_id id, transfer_direction direction, error_type type, const char *message);
} parser_callbacks;

/*
 *  General parser interface:
 */

/**
 * Create new connection and set callbacks for it
 * @param id Connection id
 * @param info Connection info (endpoint names, currently is not used
 * @param callbacks Connection callbacks
 * @return 0 if success
 */
int parser_connect(connection_id id, connection_info *info,
            parser_callbacks *callbacks);

/**
 * Mark one side of connection as disconnected.
 * If remote connection is disconnected, its state is reset
 * If local connection is disconnected, wait for remote connection state reset and deallocate
 * corresponding data structures
 * @param id Connection id
 * @param direction Transfer direction
 * @return 0 if success
 */
int parser_disconnect(connection_id id, transfer_direction direction);

/**
 * Process HTTP input data
 * @param id Connection id
 * @param direction Transfer direction
 * @param data Chunk data
 * @param length Data length
 * @return 0 if success
 */
int parser_input(connection_id id, transfer_direction direction, const char *data,
          size_t length);

/**
 * Closes connection
 * @param id Connection id
 * @return 0 if success
 */
int parser_connection_close(connection_id id);

/**
 * Utility methods
 */
http_message *http_message_clone(const http_message *source);

void http_message_free(http_message *message);

int http_message_set_method(http_message *message,
                            const char *method, size_t length);

int http_message_set_url(http_message *message,
                         const char *url, size_t length);

int http_message_set_status(http_message *message,
                            const char *status, size_t length);

int http_message_set_status_code(http_message *message, int status_code);

int http_message_set_header_field(http_message *message,
                                  const char *name, size_t name_length,
                                  const char *value, size_t value_length);

char *http_message_get_header_field(const http_message *message,
                                    const char *name, size_t length);

int http_message_add_header_field(http_message *message,
                                  const char *name, size_t length);

int http_message_del_header_field(http_message *message,
                                  const char *name, size_t length);

char *http_message_raw(const http_message *message, size_t *length);

#ifdef __cplusplus
}
#endif

#endif /* PARSER_H */
