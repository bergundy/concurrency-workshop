import socket
import logging
# import threading

from multiprocessing.pool import ThreadPool

from http_tools import blocking_handler
from request_handler import request_handler


# logging.basicConfig(level=logging.INFO)
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('0.0.0.0', 8081))
s.listen(1024)

pool = ThreadPool(100)

while True:
    peer, (host, port) = s.accept()
    logging.info('Accepted new connection from %s:%d', host, port)
    # default_headers = {'connection': 'close'}
    default_headers = {}
    pool.apply_async(blocking_handler, args=(peer, (host, port), request_handler), kwds=default_headers)
    # t = threading.Thread(target=blocking_handler, args=(peer, (host, port), request_handler), kwargs=default_headers)
    # t.daemon = True
    # t.start()
