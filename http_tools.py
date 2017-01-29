import logging
import socket
import traceback


class Request(object):
    def __init__(self, method, version, path, arguments, headers, body):
        self.method = method
        self.version = version
        self.path = path
        self.arguments = arguments
        self.headers = headers
        self.body = body


def decode_headers(data):
    header_kvs = (l.split(':', 1) for l in data.splitlines())
    return {k.lower().strip(): v.strip() for k, v in header_kvs}


def normalize_header(k):
    return '-'.join((p.title() for p in k.split('-')))


def encode_headers(headers):
    return '\r\n'.join('{}: {}'.format(normalize_header(k), v) for k, v in headers.iteritems())


def make_response(body, status_code=200, status_string='OK', **headers):
    headers.setdefault('content-type', 'text/plain')
    headers.setdefault('content-length', len(body))
    status_line = 'HTTP/1.1 {} {}'.format(status_code, status_string)
    return '\r\n'.join([status_line, encode_headers(headers), '', body])


def parse_query(query_string):
    qs = query_string.split('&')
    qs_kvs = (q.split('=', 1) for q in qs)
    return {k: v for k, v in qs_kvs}


def blocking_handler(sock, addr, request_handler, **default_headers):
    try:
        logging.info('Handling connection for %s', addr)
        data = ''
        while True:
            try:
                bytes_read = sock.recv(4096)
                if len(bytes_read) == 0:
                    break
                data += bytes_read
            except socket.error:
                logging.error('Error while reading from client socket %s', addr)
                break
            parts = data.split('\r\n', 1)
            if len(parts) == 2:  # Got request line
                request_line, rest = parts
                method, path, version = request_line.split()
                parts = rest.split('\r\n\r\n', 1)
                if len(parts) == 2:  # Got headers
                    headers_data, rest = parts
                    headers = decode_headers(headers_data)
                    content_length = int(headers.get('content-length', 0))
                    if len(rest) >= content_length:
                        body, data = rest[:content_length], rest[content_length:]
                        parts = path.split('?')
                        arguments = parse_query(parts[1]) if len(parts) == 2 else {}
                        request = Request(method, version, path, arguments, headers, body)
                        try:
                            response_body = request_handler(request)
                        except Exception:
                            response = make_response(
                                traceback.format_exc(), 500, 'Internal Server Error')
                        else:
                            response = make_response(str(response_body))
                        sock.send(response)
    finally:
        logging.info('Closing connection for %s', addr)
        sock.close()
