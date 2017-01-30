build: *.py Dockerfile
	docker build -t concurrency-workshop .
	touch build

start: build
	if [ -e start ]; then make stop; fi
	docker run -v ${PWD}:/app --ulimit nofile=262144:262144 -p 8081:8081 -p 8080:80 -p 55666:55666 -p 55667:55667 -d --privileged concurrency-workshop sh -c 'date -s "$(shell date)" && service xinetd start && service apache2 start && sleep infinity' > start
	while true; do curl http://localhost:8080/cgi-bin/mult.cgi?i=1 && exit; done &> /dev/null

trace:
	docker exec -it $(shell cat start) sh -c 'strace -f -p $$(pgrep apache2 | head -n1)'

shell: build
	docker exec -it $(shell cat start) bash

stop:
	docker rm -f $(shell cat start) || true &> /dev/null
	rm -f start

restart: stop start

test-inet-one: start
	nc localhost 55666 <<< 1

test-inet-stream: start
	echo "1\n2\n3" | nc localhost 55667

test-cgi: start
	curl http://localhost:8080/cgi-bin/mult.cgi?i=1

threaded_server:
	gcc -std=c99 -o threaded_server -pthread threaded_server.c -lhttp_parser

event_loop_server:
	gcc -std=c99 -o event_loop_server event_loop_server.c -lhttp_parser
