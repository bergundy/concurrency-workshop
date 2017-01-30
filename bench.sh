${GOPATH}/bin/hey -n $2 -c $1 -t 1 -m GET http://127.0.0.1:8081/\?i\=$3 | grep -P ' in |:'
