#!/usr/bin/python
import os

# for k, v in os.environ.items():
#     print k, '->', v


def respond():
    return int(get_query()['i']) * 7


def get_query():
    qs = os.environ['QUERY_STRING'].split('&')
    qs_kvs = (q.split('=', 1) for q in qs)
    return {k: v for k, v in qs_kvs}

response = str(respond())


print "Content-Type: text/plain"                        # Text is following
print "Content-Length: {}".format(len(response) + 1)    # 1 for newline
print                                                   # blank line, end of headers
print response
