#include "http.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <stdbool.h>

void initialize_resp_struct(HTTPResponse *resp) {
  resp->version = NULL;
  resp->code = NULL;
  resp->content_length = NULL;
  resp->content_type = NULL;
  resp->payload = NULL;
}

void send_message(HTTPResponse* resp, int success_or_not, int client_sock) {
  int size = sizeof(*resp);
  char server_message[1024];
  memset(server_message, '\0', sizeof(server_message));
  if (success_or_not == 0) {
    snprintf(server_message, sizeof server_message, "%s %s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", resp->version, resp->code, resp->content_length, resp->content_type);
    if (send(client_sock, server_message, strlen(server_message), 0) < 0) {
        printf("Can't send\n");
        exit(1);
    }
    char pl [resp->content_length];
    memcpy(pl, resp->payload, resp->content_length);
    int sent_bytes = 0;
    if ((sent_bytes = send(client_sock, resp->payload, resp->content_length, 0)) < 0) {
        printf("Can't send\n");
        exit(1);
    }  
    printf("- Sent a HTTP response containing desired item (%d bytes) to client.\n", sent_bytes);
    memset(server_message, '\0', sizeof(server_message));
    
  } else {
    snprintf(server_message, sizeof server_message, "%s %s\r\n\r\n", resp->version, resp->code);
    if (send(client_sock, server_message, strlen(server_message), 0) < 0) {
        printf("Can't send\n");
        exit(1);
    }
    memset(server_message, '\0', sizeof(server_message));
  }
}

int find_file(const char *pattern, DIR *d) {
    char* thing = strdup(pattern);
    if (thing[0] == '/') 
      memmove(thing, thing+1, strlen(thing));                                                                                                                                     
    struct dirent *de;                                                                                                                                                                 
    int found = -1;                                                                                                                                                                    
    while ( (de = readdir(d)) != NULL) {                                                                                                                                              
        if (de->d_type == DT_REG && strstr(de->d_name, thing) != NULL) { // DT_REG is a regular file                                                                                                         
            found = 0;   
            printf("- File requested (%s) has been found.\n", de->d_name); 
            free(thing);                                                                                                                                                         
            break;                                                                                                                                                                     
        }                                                                                                                                                                              
    }                                                                                                                                                                                  
    return found;                                                                                                                                                                      
} 

void *client_thread(void *vptr) {
  int fd = *((int *)vptr);
  printf("\n---> Client connected. Socket #%d\n", fd);
  HTTPRequest *req = (HTTPRequest *) malloc(sizeof(HTTPRequest));
  int bytes_read_from_socket = httprequest_read(req, fd);
  printf("- HTTP request of %d bytes has been read in from the client socket.\n", bytes_read_from_socket);
  HTTPResponse *resp = (HTTPResponse *) malloc(sizeof(HTTPResponse));
  initialize_resp_struct(resp);
  printf("- Type of HTTP request: %s\n", req->action);
  resp->version = "HTTP/1.0";

  DIR *dir = opendir("./static");                                                                                                                                                             
  if (strcmp(req->path, "/") == 0) {
    req->path = "/index.html";
  }
  char* ext = strrchr(req->path, '.');
  const char *file_type = ext+1; 
  // printf("%s\n", file_type);
  struct dirent *de;                                                                                                                                                                 
  int found = -1;                                                                                                                                                                   
  if (find_file(req->path, dir) == 0) { // Requested file exists.
    char buf[0x100];
    snprintf(buf, sizeof(buf), "./static%s", req->path);
    printf("- File path of requested item: %s\n", buf);
    FILE *f = fopen(buf, "r");

    int total_bytes_read = 0;
    bool still_to_read = true;
    resp->payload = (void*)malloc(256);
    int counter = 1;
    // Read the file requested's bytes into the payload.
    while (still_to_read) {
      resp->payload = realloc(resp->payload, 256*counter);
      int bytes_read = fread(resp->payload+total_bytes_read, sizeof(char), 256, f);
      total_bytes_read += bytes_read;
      counter++;
      if (bytes_read < 256) {
        break; // Last bytes read in; all in buffer.
      }
    }
    // File fully read in.
    resp->code = "200 OK";
    resp->content_length = total_bytes_read;
    if (strcmp(file_type, "png") == 0) {
      resp->content_type = "image/png";
    } else if (strcmp(file_type, "html") == 0){
      resp->content_type =  "text/html";
    }
    send_message(resp, 0, fd);
  } else { // File not found.
    resp->version = "HTTP/1.0";
    resp->code = "404 Not Found";
    send_message(resp, 1, fd);
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("##### Arguments incorrect. Correct program usage is: %s <port>\n", argv[0]);
    return 1;
  }
  int port = atoi(argv[1]);
  printf("##### Binding to port %d. Visit http://localhost:%d/ to interact with the server!\n", port, port);

  // socket:
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    printf("setsockopt(SO_REUSEADDR) failed");

  // bind:
  struct sockaddr_in server_addr, client_address;
  memset(&server_addr, 0x00, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);  
  bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr));

  // listen:
  listen(sockfd, 10);

  // accept:
  socklen_t client_addr_len;
  while (1) {
    int *fd = malloc(sizeof(int));
    client_addr_len = sizeof(struct sockaddr_in);
    *fd = accept(sockfd, (struct sockaddr *)&client_address, &client_addr_len);
    // printf("Client connected (fd=%d)\n", *fd);

    pthread_t tid;
    pthread_create(&tid, NULL, client_thread, fd);
    pthread_detach(tid); // does what?
  }

  return 0;
}

// docker
// autograding
// function params?
// detach