#define die(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }
#define log(...) { fprintf(stderr, __VA_ARGS__); }

/* const char *response_template = "HTTP/1.1 200 OK Content-Type: text/plain\r\nConnection: close\r\nContent-Length: %d\r\n\r\n%d"; */
const char *response_template = "HTTP/1.1 200 OK Content-Type: text/plain\r\nContent-Length: %d\r\n\r\n%d";

int fib(int n) {
    switch (n) {
    case 1:
        return 1;
    case 0:
        return 0;
    default:
        return fib(n - 1) + fib(n - 2);
    }
}
