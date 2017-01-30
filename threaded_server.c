#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <http_parser.h>
#include "common.h"

struct request_context {
    int fd;
    int i;
};

int num_connected = 0;
int num_handling = 0;

int on_url(http_parser *parser, const char *at, size_t length) {
    struct http_parser_url u = {0};
    char query[1024];
    struct request_context *ctx = (struct request_context *)parser->data;
    http_parser_parse_url(at, length, 0, &u);
    uint16_t len = u.field_data[UF_QUERY].len;
    memcpy(query, at + u.field_data[UF_QUERY].off, len);
    query[len] = '\0';
    ctx->i = atoi(query + 2);
    /* __sync_add_and_fetch(&num_handling, 1); */
    /* log("On URL - query: %s\n", query); */
    return 0;
}

int on_message_begin(http_parser *parser) {
    return 0;
}

int on_message_complete(http_parser *parser) {
    /* log("Message complete\n"); */
    struct request_context *ctx = (struct request_context *)parser->data;
    char response[1024];
    int result = fib(ctx->i);
    sprintf(response, "%d", result);
    sprintf(response, response_template, strlen(response), result);
    send(ctx->fd, response, strlen(response), 0);
    /* int val = __sync_add_and_fetch(&num_handling, -1); */
    /* log("Requests in flight: %d\n", val); */
    return 0;
}

int noop_cb(http_parser *parser) { return 0; }
int noop_data_cb(http_parser *parser, const char *at, size_t length) { return 0; }

void *handle_client(void *client_fd_ptr) {
    /* int val = __sync_add_and_fetch(&num_connected, 1); */
    /* log("Num connected: %d\n", val); */
    http_parser_settings settings =
        {.on_message_begin = on_message_begin
        ,.on_header_field = noop_data_cb
        ,.on_header_value = noop_data_cb
        ,.on_url = on_url
        ,.on_status = noop_data_cb
        ,.on_body = noop_data_cb
        ,.on_headers_complete = noop_cb
        ,.on_message_complete = on_message_complete
        ,.on_chunk_header = noop_cb
        ,.on_chunk_complete = noop_cb
        };
    http_parser *parser = malloc(sizeof(http_parser));
    http_parser_init(parser, HTTP_REQUEST);
    int client_fd = *(int *)client_fd_ptr;
    struct request_context ctx = {client_fd, -1};
    parser->data = &ctx;
    int err;
    size_t len = 80 * 1024, nparsed;
    char buf[len];
    ssize_t recved;

    while (1) {
        recved = recv(client_fd, buf, len, 0);
        if (recved == 0) {
            /* log("Client disconnected\n"); */
            goto closing;
        }
        if (recved < 0) {
            /* perror("Client read failed"); */
            goto cleanup;
        }

        /* __sync_add_and_fetch(&num_handling, 1); */
        nparsed = http_parser_execute(parser, &settings, buf, recved);
        if (parser->upgrade) {
            log("Unsupported operation UPGRADE\n");
            goto closing;
        } else if (nparsed != recved) {
            log("Error parsing input from client\n");
            goto closing;
        }
    }

closing:
    err = close(client_fd);
    if (err != 0) {
        /* log("Failed to close socket\n"); */
    }

cleanup:
    free(parser);
    /* val = __sync_add_and_fetch(&num_connected, -1); */
    /* log("Num connected: %d\n", val); */
    return NULL;
}

int main (int argc, char *argv[]) {
    int port = 8081;

    int server_fd, client_fd, err;
    struct sockaddr_in server, client;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) die("Could not create socket\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof opt_val);

    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (err < 0) die("Could not bind socket\n");

    err = listen(server_fd, 1024);
    if (err < 0) die("Could not listen on socket\n");

    printf("Server is listening on %d\n", port);

    pthread_attr_t attr;
    err = pthread_attr_init(&attr);
    if (err != 0) {
        die("Failed to initialize pthread attr");
    }
    err = pthread_attr_setdetachstate(&attr, 1);
    if (err != 0) {
        die("Failed to set pthread attr detached state");
    }

    while (1) {
        socklen_t client_len = sizeof(client);
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) {
            perror("Failed to accept new connection");
        } else {
            pthread_t t;
            pthread_create(&t, &attr, handle_client, &client_fd);
        }
        /* handle_client(client_fd); */
    }

    return 0;
}
