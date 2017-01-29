FROM ubuntu:14.04
RUN apt-get update && apt-get install -y \
    build-essential \
    strace          \
    python          \
    xinetd          \
    apache2         \
    curl            \
    telnet

# inetd
RUN echo mult 55666/tcp >> /etc/services
RUN echo multstream 55667/tcp >> /etc/services
COPY mult.inetd /etc/xinetd.d/mult

# apache
RUN a2enmod cgi
COPY apache.conf /etc/apache2/sites-available/000-default.conf

# app
RUN mkdir /app
COPY *.py /app/
RUN cp /app/multcgi.py /usr/lib/cgi-bin/mult.cgi && chown www-data:www-data /usr/lib/cgi-bin/mult.cgi
