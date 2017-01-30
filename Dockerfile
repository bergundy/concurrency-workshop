FROM ubuntu:14.04
RUN apt-get update && apt-get install -y software-properties-common && add-apt-repository -y ppa:ubuntu-lxc/lxd-stable
RUN apt-get update && apt-get install -y \
    build-essential \
    strace          \
    python          \
    xinetd          \
    apache2         \
    curl            \
    lsof            \
    git             \
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
 && tar xvzf http-parser.tar.gz                                                               \
 && cd http-parser*                                                                           \
 && make install PREFIX=/usr                                                                  \
 && cd ..                                                                                     \
 && rm -rf http-parser*

# wrk
RUN cd /tmp                                                              \
 && curl -L -o wrk.tar.gz https://github.com/wg/wrk/archive/4.0.2.tar.gz \
 && tar xvzf wrk.tar.gz                                                  \
 && cd wrk*                                                              \
 && make                                                                 \
 && mv wrk /usr/bin                                                      \
 && cd ..                                                                \
 && rm -rf wrk*

# golang
RUN cd /usr/local                                                                         \
 && curl -L -o go.tar.gz https://storage.googleapis.com/golang/go1.7.1.linux-amd64.tar.gz \
 && tar xvzf go.tar.gz                                                                    \
 && ln -s /usr/local/go/bin/go /usr/bin/                                                  \
 && rm go.tar.gz
ENV GOPATH=/gohome
RUN echo GOPATH=$GOPATH > ~/.bashrc

# hey
RUN go get -u github.com/rakyll/hey

# app
RUN mkdir /app
WORKDIR /app
COPY *.py /app/
RUN cp /app/multcgi.py /usr/lib/cgi-bin/mult.cgi && chown www-data:www-data /usr/lib/cgi-bin/mult.cgi
COPY *.c /app/
