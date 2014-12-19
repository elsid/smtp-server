#!/bin/bash

COUNT=1

valgrind --leak-check=full --trace-children=yes --show-leak-kinds=all \
    bin/smtp-server etc/test_memory.cfg 2>&1 &

PID=$!

sleep 1

test/loadtest_session.bash 4 100
test/loadtest_request.bash 4 100
test/loadtest.bash 2 100

sleep 1

kill -SIGINT $PID

wait
