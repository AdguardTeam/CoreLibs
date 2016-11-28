/*
 *  HTTP parser API.
 */
#ifndef HTTP_PARSER_PARSER_H
#define HTTP_PARSER_PARSER_H

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

typedef unsigned long connection_id_t;

/*  Connection is represented by two abstract endpoints, which are titled for
    easy identification by C-strings. */
#define ENDPOINT_MAXLEN 255

typedef struct {
    char endpoint_1[ENDPOINT_MAXLEN];
    char endpoint_2[ENDPOINT_MAXLEN];
} connection_info;

/**
 * Transfer direction
 * IN - Data from remote server
 * OUT - Data from client
 */
typedef enum {
    DIRECTION_IN = 0,
    DIRECTION_OUT
} transfer_direction_t;

/**
 * Parser error type
 * HTTP - http_parser error
 * DECODE - zlib error
 */
typedef enum {
    PARSER_HTTP_ERROR = 0,
    PARSER_DECODE_ERROR
} error_type_t;

/**
 * Recommended chunk size for zlib inflate
 */
#define ZLIB_DECOMPRESS_CHUNK_SIZE 262144
#define ZLIB_COMPRESS_CHUNK_SIZE 65536

/*  User-defined callbacks.
    To inform parser about needed action, other then default "nothing to do", 
    callback should return non-zero code;
    In all other cases you should return 0. */
typedef int (*parser_cb)(connection_id_t id, void *data, size_t length);

typedef struct {
    /**
     * HTTP request received callback
     * @param id Connection id
     * @param header http_header structure
     * @return Non-null value if we are skipping this request
     */
    int (*http_request_received)(connection_id_t id, void *header);
    /**
     * HTTP request body started callback
     * @param id Connection id
     * @return Body compression type
     *          0 means uncompressed
     *          1 means zlib compression with deflate format
     *          2 means zlib compression with gzip format
     */
    int (*http_request_body_started)(connection_id_t id);
    /**
     * HTTP request body data callback
     * @param id Connection id
     * @param data Chunk of data (doesn't correspond to any HTTP chunk, just part of decoded stream)
     * @param length Length of data chunk
     */
    void (*http_request_body_data)(connection_id_t id, const char *data, size_t length);
    /**
     * HTTP request body finished callback
     * @param id Connection id
     */
    void (*http_request_body_finished)(connection_id_t id);
    /**
     * HTTP response received callback
     * @param id Connection id
     * @param header http_header structure
     * @return Non-null value if we are skipping this request/response
     */
    int (*http_response_received)(connection_id_t id, void *header);
    /**
     * HTTP response body started callback
     * @param id Connection id
     * @return Body compression type
     *          0 means uncompressed
     *          1 means zlib compression with deflate format
     *          2 means zlib compression with gzip format
     */
    int (*http_response_body_started)(connection_id_t id);
    /**
     * HTTP response body data callback
     * @param id Connection id
     * @param data Chunk of data (doesn't correspond to any real HTTP chunk, just part of decoded stream)
     * @param length Length of data chunk
     */
    void (*http_response_body_data)(connection_id_t id, const char *data, size_t length);
    /**
     * HTTP response body finished callback
     * @param id Connection id
     */
    void (*http_response_body_finished)(connection_id_t id);

    /**
     * HTTP parser error
     * @param id Connection id
     * @param type Error type (HTTP parser/decode)
     * @param message Error message
     */
    void (*parse_error)(connection_id_t id, transfer_direction_t direction, error_type_t type, const char *message);
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
int parser_connect(connection_id_t id, connection_info *info,
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
int parser_disconnect(connection_id_t id, transfer_direction_t direction);

/**
 * Process HTTP input data
 * @param id Connection id
 * @param direction Transfer direction
 * @param data Chunk data
 * @param length Data length
 * @return 0 if success
 */
int parser_input(connection_id_t id, transfer_direction_t direction, const char *data,
          size_t length);

/**
 * Closes connection
 * @param id Connection id
 * @return 0 if success
 */
int parser_connection_close(connection_id_t id);

/**
 * Utility methods
 */
/**
 * Create new connection and set callbacks for it
 * @param id Connection id
 * @param info Connection info (endpoint names, currently is not used
 * @param callbacks Connection callbacks
 * @return 0 if success
 */
int parser_connect(connection_id_t id, connection_info *info,
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
int parser_disconnect(connection_id_t id, transfer_direction_t direction);

/**
 * Process HTTP input data
 * @param id Connection id
 * @param direction Transfer direction
 * @param data Chunk data
 * @param length Data length
 * @return 0 if success
 */
int parser_input(connection_id_t id, transfer_direction_t direction, const char *data,
                 size_t length);

/**
 * Closes connection
 * @param id Connection id
 * @return 0 if success
 */
int parser_connection_close(connection_id_t id);

/**
 * Utility methods
 */
/**
 * Makes copy of HTTP message
 * @param source Pointer to original message
 * @return Pointer to cloned message. Should be freed by http_message_free()
 */
http_message *http_message_clone(const http_message *source);

/**
 * Deallocates HTTP message
 * @param message Pointer to message
 */
void http_message_free(http_message *message);

/**
 * Sets method field of request line of HTTP message
 * @param message Pointer to HTTP message
 * @param method Method name (character array)
 * @param length Length of method name character array
 * @return 0 if success
 */
int http_message_set_method(http_message *message,
                            const char *method, size_t length);

/**
 * Sets URL field of request line of HTTP message
 * @param message Pointer to HTTP message
 * @param url URL to set (character array)
 * @param length Length of URL character array
 * @return 0 if success
 */
int http_message_set_url(http_message *message,
                         const char *url, size_t length);

/**
 * Sets status string of response line of HTTP message
 * @param message Pointer to HTTP message
 * @param status Status string (character array)
 * @param length Length of status string characted array
 * @return 0 if success
 */
int http_message_set_status(http_message *message,
                            const char *status, size_t length);

/**
 * Sets status code of response line of HTTP message
 * @param message Pointer to HTTP message
 * @param status_code Status code (integer)
 * @return 0 if success
 */
int http_message_set_status_code(http_message *message, int status_code);

/**
 * Sets header field at header section of HTTP message
 * @param message Pointer to HTTP message
 * @param name Field name (character array)
 * @param name_length Length of field name character array
 * @param value Value (character array; may be empty or null)
 * @param value_length Length of value character array
 * @return 0 if success
 */
int http_message_set_header_field(http_message *message,
                                  const char *name, size_t name_length,
                                  const char *value, size_t value_length);

/**
 * Gets header field value of header section of HTTP message
 * @param message Ponter to HTTP message
 * @param name Field name (character array)
 * @param length Length of field name character array
 * @return 0 if success
 */
const char * http_message_get_header_field(const http_message *message,
                                           const char *name, size_t length);

/**
 * Adds new header field at header section of HTTP message
 * Value if set by http_message_set_header_field() function
 * @param message Pointer to HTTP message
 * @param name Field name (character array)
 * @param length Length of field name character array
 * @return 0 if success
 */
int http_message_add_header_field(http_message *message,
                                  const char *name, size_t length);

/**
 * Deletes header field from header section of HTTP message
 * @param message Pointer to HTTP message
 * @param name Field name (character array)
 * @param length Length of field name character array
 * @return 0 if success
 */
int http_message_del_header_field(http_message *message,
                                  const char *name, size_t length);

/**
 * Serializes HTTP message header section, including request/response line,
 * header fields and the ending CRLF
 * @param message Pointer to HTTP message
 * @param length Pointer to size_t variable where length of output will be written
 * @return Character array containing serialized HTTP message
 */
char *http_message_raw(const http_message *message, size_t *length);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PARSER_PARSER_H */
