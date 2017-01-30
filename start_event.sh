#!/bin/bash -e

for x in `seq 4`; do
    ./event_loop_server &
done

function cleanup {
    pkill -P $$
}
trap cleanup EXIT

wait
