#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include "http.h"

void initialize_struct(HTTPRequest *req) {
  req->action = NULL;
  req->path = NULL;
  req->version = NULL;
  req->payload = NULL;
  req->head_header = NULL;
}

void first_header(HTTPRequest *req, char *header) {
    char* token;
    char* rest = header;
    int component_count = 0;
    while ((token = strtok_r(rest, " ", &rest)) && component_count < 3) {
        switch(component_count) {
            case 0 :
              req->action = strdup(token); // 4 Bytes copied... GET\n. Remember to free!
              break;
            case 1 :
              req->path = strdup(token);
              break;
            case 2 :
              req->version = strdup(token);
              break;
        }
        component_count++;
    }
}

void next_header(HTTPRequest *req, char *header) {
    char* rest = strdup(header);
    char* pointer_to_free = rest;
    char* header_key = strtok(rest, " ");
    header_key[strlen(header_key)-1] = '\0';

    header_t* header_node = (header_t *) malloc(sizeof(header_t));
    header_node->name = strdup(header_key);
    char* value = strtok(NULL, " ");
    header_node->value = strdup(value);
    header_node->next = NULL;
    if (req->head_header == NULL) {
      req->head_header = header_node;
    } else {
      header_t* buffer = req->head_header;
      req->head_header = header_node;
      header_node->next = buffer;
    }
    free(pointer_to_free);
}

/**
 * httprequest_parse_headers
 * 
 * Populate a `req` with the contents of `buffer`, returning the number of bytes used from `buf`.
 */
ssize_t httprequest_parse_headers(HTTPRequest *req, char *buffer, ssize_t buffer_len) {
  initialize_struct(req);
  int number_of_headers = 0;
  for (int i = 0; buffer_len; i++) {
    int line_length = 0;
    char header[2000];
    memset(header, '\0', 2000);

    if (buffer[i] == '\r') {
      // The byte payload has been encountered (and there is one); i is currently at the second `\r`.
      // Check if there's a content length header node.
      header_t* current_header_node = req->head_header;
      int content_length = 0;
      while (current_header_node != NULL) { // Search for a content-length node.
        // current_header_node->name = (const char *) current_header_node->name;
        if (strcmp(current_header_node->name, "Content-Length") == 0) { // There's a content-length node.
          content_length = atoi(current_header_node->value);
          break;
        }
        current_header_node = current_header_node->next;
      }
      if (content_length > 0) {
        req->payload = (void*) malloc(content_length);
        memcpy(req->payload, buffer+i+2, content_length); // Need to pass pointer, not the value! ([])
        // printf("%s\n", req->payload);
        return buffer_len;
        // ((char *) payload)[i] = buffer[i];
      } else {
        return buffer_len; // Reached end of headers and there's no payload, so return.
      }
    }

    while (buffer[i] != '\r' && i < buffer_len) { // Encountered end of header.
      memcpy(header+line_length, buffer + i++, 1);
      // header[line_length] = buffer[i++];
      line_length++;
    }
    if (number_of_headers == 0) {
      first_header(req, header);
    } else {
      next_header(req, header); // Need to malloc new field.
    }
    number_of_headers++;
    i += 1;
    // printf("Header %d read in successfully: %s.\n", number_of_headers, header);
    memset(header, '\0', sizeof(header));
 }
 return buffer_len;
}


/**
 * httprequest_read
 * 
 * Populate a `req` from the socket `sockfd`, returning the number of bytes read to populate `req`.
 */
ssize_t httprequest_read(HTTPRequest *req, int sockfd) {
  int total_bytes_read = 0;
  int bytes_to_read_at_a_time = 65536;
  // char buffer[6000000] = { 0 };
  int INIT_SIZE = 65536;
  char* buffer = (char*)malloc(INIT_SIZE);
  bool still_to_read = true;
  int counter = 1;
  while (still_to_read) {
    buffer = realloc(buffer, INIT_SIZE*counter);
    int valread = read(sockfd, buffer + total_bytes_read, bytes_to_read_at_a_time);
    // printf("%d bytes read.\n", valread);
    // printf("%s\n", buffer);
    total_bytes_read += valread;
    counter++;
    if (valread < bytes_to_read_at_a_time) {
      break; // Last bytes read in; all in buffer.
    }
  }
  // printf("---> Client's message:\n%s\n", buffer);
  // printf("Buffer: %s\n", buffer);
  httprequest_parse_headers(req, buffer, total_bytes_read);
  free(buffer);
  return total_bytes_read;
}


/**
 * httprequest_get_action
 * 
 * Returns the HTTP action verb for a given `req`.
 */
const char *httprequest_get_action(HTTPRequest *req) {
  return req->action;
}


/**
 * httprequest_get_header
 * 
 * Returns the value of the HTTP header `key` for a given `req`.
 */
const char *httprequest_get_header(HTTPRequest *req, const char *key) {
  header_t* current_header_node = req->head_header;
  while (current_header_node != NULL) { // Search for a content-length node.
    if (strcmp(current_header_node->name, key) == 0) { // There's a content-length node.
      return current_header_node->value;
    }    
    current_header_node = current_header_node->next;
  }  
  return NULL;
}


/**
 * httprequest_get_path
 * 
 * Returns the requested path for a given `req`.
 */
const char *httprequest_get_path(HTTPRequest *req) {
  return req->path;
}


/**
 * httprequest_destroy
 * 
 * Destroys a `req`, freeing all associated memory.
 */
void httprequest_destroy(HTTPRequest *req) {
  header_t* current_header_node = req->head_header;
  while (current_header_node != NULL) { // Search for a content-length node.
    header_t* node_to_free = current_header_node;
    current_header_node = current_header_node->next;
    free((char*) node_to_free->name);
    free((char*) node_to_free->value);
    free(node_to_free);
  }
  free((char*) req->action);
  free((char*) req->path);
  free((char*) req->payload);
  free((char*) req->version);
}

// strcpy(x,y) will deference the memory at x (and copy y into it); if x is NULL (or a NP) you'll segfault.