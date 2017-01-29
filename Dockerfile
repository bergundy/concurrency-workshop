FROM ubuntu:14.04
RUN apt-get update && apt-get install -y \
    build-essential \
    strace          \
    python          \
    xinetd          \
    apache2         \
    curl            \
    lsof            \
    telnet

# inetd
RUN echo mult 55666/tcp >> /etc/services
RUN echo multstream 55667/tcp >> /etc/services
COPY mult.inetd /etc/xinetd.d/mult

# apache
RUN a2enmod cgi
COPY apache.conf /etc/apache2/sites-available/000-default.conf

# http-parser
RUN cd /tmp                                                                                   \
 && curl -L -o http-parser.tar.gz https://github.com/nodejs/http-parser/archive/v2.7.1.tar.gz \
 && cd /tmp                                                                                   \
 && tar xvzf http-parser.tar.gz                                                               \
 && cd http-parser*                                                                           \
 && make install PREFIX=/usr                                                                  \
 && cd ..                                                                                     \
 && rm -rf http-parser*

# app
RUN mkdir /app
WORKDIR /app
COPY *.py /app/
RUN cp /app/multcgi.py /usr/lib/cgi-bin/mult.cgi && chown www-data:www-data /usr/lib/cgi-bin/mult.cgi
COPY *.c /app/
RUN gcc -o threaded_server -pthread threaded_server.c -lhttp_parser
