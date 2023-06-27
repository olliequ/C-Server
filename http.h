#pragma once
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif


struct _HTTPRequest {
  const char *action;
  const char *path;
  const char *version;
  const void *payload;
  struct _header_t* head_header;
};
typedef struct _HTTPRequest HTTPRequest;

struct _HTTPResponse {
  const char *version;
  const char *code;
  int content_length;
  const char *content_type;
  const void *payload;
  struct _header_t* head_header;
};
typedef struct _HTTPResponse HTTPResponse;

typedef struct _header_t {
    const char* name;
    const char* value;
    struct _header_t* next;
} header_t;


ssize_t httprequest_read(HTTPRequest *req, int sockfd);
ssize_t httprequest_parse_headers(HTTPRequest *req, char *buffer, ssize_t buffer_len);
const char *httprequest_get_action(HTTPRequest *req);
const char *httprequest_get_header(HTTPRequest *req, const char *key);
const char *httprequest_get_path(HTTPRequest *req);
void httprequest_destroy(HTTPRequest *req);

#ifdef __cplusplus
}
#endif