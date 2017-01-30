#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <http_parser.h>
#include "common.h"


#define MAX_EVENTS 100

struct request_context {
    int valid;
    int i;
};

struct client {
    int fd;
    int epoll_fd;
    int out_len;
    int events;
    char *out_buf;
    http_parser *parser;
    struct request_context *ctx;
};

struct client *new_client(int epoll_fd, int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    struct client *cli = (struct client *)malloc(sizeof(struct client));
    if (cli == NULL) {
        return NULL;
    }
    cli->fd = fd;
    http_parser *parser = malloc(sizeof(http_parser));
    if (parser == NULL) {
        goto cleanup;
    }
    http_parser_init(parser, HTTP_REQUEST);
    parser->data = cli;
    cli->parser = parser;
    struct request_context *ctx = (struct request_context *)malloc(sizeof(struct request_context));
    if (ctx == NULL) {
        goto cleanup_parser;
    }
    cli->ctx = ctx;
    cli->epoll_fd = epoll_fd;
    cli->out_len = 0;
    cli->out_buf = (char *)malloc(sizeof(char) * 100 * 1024);
    if (cli->out_buf == NULL) {
        goto cleanup_ctx;
    }

    struct epoll_event ev;
    ev.data.ptr = cli;
    ev.events = cli->events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

    return cli;

cleanup_ctx:
    free(cli->ctx);
cleanup_parser:
    free(cli->parser);
cleanup:
    free(cli);
    return NULL;
}

void delete_client(struct client *cli) {
    if (epoll_ctl(cli->epoll_fd, EPOLL_CTL_DEL, cli->fd, NULL) == -1) {
        perror("Failed to delete fd epoll");
    }
    free(cli->out_buf);
    free(cli->ctx);
    free(cli->parser);
    free(cli);
}

int client_try_write(struct client *cli, const char *buf, size_t len) {
    if (cli->events & EPOLLOUT) {
        // pending write
        memcpy(cli->out_buf + cli->out_len, buf, len);
        cli->out_len += len;
    } else {
        int sent = send(cli->fd, buf, len, 0);
        if (sent < len) {
            if (sent == -1 && errno != EAGAIN) {
                perror("Failed to write");
                delete_client(cli);
                return -1;
            } else {
                log("Sent partial data\n");
                sent = sent == -1 ? 0 : sent;
                memcpy(cli->out_buf + cli->out_len, buf + sent, len - sent);
                cli->out_len += len - sent;
                struct epoll_event ev;
                ev.data.ptr = cli;
                cli->events |= EPOLLOUT;
                ev.events = cli->events;
                epoll_ctl(cli->epoll_fd, EPOLL_CTL_MOD, cli->fd, &ev);
            }
        }
    }
    return 0;
}

int client_flush(struct client *cli) {
    int sent = send(cli->fd, cli->out_buf, cli->out_len, 0);
    if (sent == -1) {
        if (errno != EAGAIN) {
            delete_client(cli);
            return -1;
        }
    } else if (sent < cli->out_len) {
        cli->out_len -= sent;
        memcpy(cli->out_buf, cli->out_buf + sent, cli->out_len);
    } else {
        cli->out_len = 0;
        struct epoll_event ev;
        ev.data.ptr = cli;
        cli->events &= ~EPOLLOUT;
        ev.events = cli->events;
        epoll_ctl(cli->epoll_fd, EPOLL_CTL_MOD, cli->fd, &ev);
    }

    return 0;
}

int num_connected = 0;
int num_handling = 0;

int on_url(http_parser *parser, const char *at, size_t length) {
    struct http_parser_url u = {0};
    char query[1024];
    struct request_context *ctx = ((struct client *)parser->data)->ctx;
    http_parser_parse_url(at, length, 0, &u);
    uint16_t len = u.field_data[UF_QUERY].len;
    memcpy(query, at + u.field_data[UF_QUERY].off, len);
    query[len] = '\0';
    ctx->valid = 1;
    ctx->i = atoi(query + 2);
    /* log("On URL - query: %s\n", query); */
    return 0;
}

int on_message_begin(http_parser *parser) {
    /* int val __sync_add_and_fetch(&num_handling, 1); */
    /* log("Message begin: %d\n", val); */
    struct request_context *ctx = ((struct client *)parser->data)->ctx;
    ctx->valid = 0;
    ctx->i = -1;
    return 0;
}

int on_message_complete(http_parser *parser) {
    /* log("Message complete\n"); */
    struct client *client = (struct client *)parser->data;
    char response[1024];
    int result = fib(client->ctx->i);
    sprintf(response, "%d", result);
    sprintf(response, response_template, strlen(response), result);
    /* int val = __sync_add_and_fetch(&num_handling, -1); */
    /* log("Requests in flight: %d\n", val); */
    return client_try_write(client, response, strlen(response));
}

int noop_cb(http_parser *parser) { return 0; }
int noop_data_cb(http_parser *parser, const char *at, size_t length) { return 0; }

int handle_read(struct client *client, const char *buf, size_t recved) {
    /* __sync_add_and_fetch(&num_connected, 1); */
    static http_parser_settings settings =
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
    http_parser *parser = client->parser;

    size_t nparsed = http_parser_execute(parser, &settings, buf, recved);
    if (parser->upgrade) {
        log("Unsupported operation UPGRADE\n");
        return -1;
    } else if (nparsed != recved) {
        log("Error parsing input from client\n");
        return -1;
    }
    return 0;
}

int main (int argc, char *argv[]) {
    int port = 8081;

    int server_fd, client_fd, err;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) die("Could not create socket\n");

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

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

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return EXIT_FAILURE;
    }

    struct epoll_event ev;
    ev.data.ptr = NULL;
    ev.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    struct epoll_event events[MAX_EVENTS] = {0};
    size_t len = 80 * 1024, nparsed;
    char buf[len];
    ssize_t recved;

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("Epoll wait");
            return EXIT_FAILURE;
        }
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.ptr == NULL) {  // Server fd
                client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
                if (client_fd < 0) {
                    if (errno == EAGAIN) {
                        perror("Failed to accept new connection");
                    }
                } else {
                    /* log("Accepted new connection\n"); */
                    struct client *cli = new_client(epoll_fd, client_fd);
                    if (cli == NULL) {
                        die("Failed to create new client\n");
                    }
                }
            } else {
                struct client *cli = (struct client *)events[n].data.ptr;
                if (events[n].events & EPOLLIN) {
                    int finished = 0;
                    while (!finished) {
                        recved = recv(cli->fd, buf, len, 0);
                        if (recved == 0) {
                            /* log("Client disconnected\n"); */
                            delete_client(cli);
                            close(cli->fd);
                            finished = 1;
                        } else if (recved < 0) {
                            finished = 1;
                            if (errno == EAGAIN) {
                                log("Read would block\n");
                            } else {
                                perror("Client read failed");
                                delete_client(cli);
                            }
                        } else {
                            finished = recved < len;
                            if (handle_read(cli, buf, recved) == -1) {
                                log("Handle read failed\n");
                                delete_client(cli);
                                close(cli->fd);
                                finished = 1;
                            }
                        }
                    }
                }
                if (events[n].events & EPOLLOUT) {
                    // ready for writing
                    client_flush(cli);
                }
            }
        }
    }

    return 0;
}
