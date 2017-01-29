#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define READ_SIZE 4096
#define MAX_BUFFER_SIZE 40960
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }
#define log(...) { fprintf(stderr, __VA_ARGS__); }

int min(int a, int b) {
    if (b < a) {
        return b;
    }
    return a;
}

void handle_client(int client_fd, struct sockaddr_in client) {
    int err, request_size;
    int start_offset = 0;
    int buf_size = 0;
    char buf[MAX_BUFFER_SIZE];
    char *headers = NULL, *body = NULL, *method = NULL, *path = NULL, *version = NULL;

    while (1) {
        if (MAX_BUFFER_SIZE - buf_size <= 0) {
            log("Client buffer full\n");
            close(client_fd);
        }
        int read = recv(client_fd, buf + start_offset, min(READ_SIZE, MAX_BUFFER_SIZE - buf_size), 0);

        if (!read) break; // done reading
        if (read < 0) {
            perror("Client read failed");
            break;
        }

        start_offset += read;
        buf_size += read;

        char *headers = strnstr(buf, "\r\n", buf_size);
        if (headers != NULL) {
            request_size = headers - buf;
            *headers = '\0';
            headers += 2;
            char *delim = memchr(buf, ' ', request_size);
            if (delim != NULL) {
                *delim = '\0';
                char *method = buf;
                char *path = delim + 1;
                delim = memchr(path, ' ', request_size - (path - buf));
                if (delim != NULL) {
                    *delim = '\0';
                    char *version = delim + 1;
                    /* TODO: reset buf */
                    char *body = strnstr(headers, "\r\n\r\n", buf_size - request_size - 2);
                    if (body != NULL) {
                    }

                    err = send(client_fd, buf, request_size, 0);
                    if (err < 0) on_error("Client write failed\n");
                    continue;
                }
            }
        }
        close(client_fd);
        break;
    }
}

int main (int argc, char *argv[]) {
    if (argc < 2) on_error("Usage: %s [port]\n", argv[0]);
  
    int port = atoi(argv[1]);
  
    int server_fd, client_fd, err;
    struct sockaddr_in server, client;
  
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) on_error("Could not create socket\n");
  
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
  
    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);
  
    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (err < 0) on_error("Could not bind socket\n");
  
    err = listen(server_fd, 128);
    if (err < 0) on_error("Could not listen on socket\n");
  
    printf("Server is listening on %d\n", port);
  
    while (1) {
        socklen_t client_len = sizeof(client);
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) on_error("Could not establish new connection\n");
        handle_client(client_fd, client);
    }
  
    return 0;
}
