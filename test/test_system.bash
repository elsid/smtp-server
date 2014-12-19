#!/bin/bash

bin/smtp-server etc/test_system.cfg > /dev/null 2>&1 &

PID=$!

test/test_system.py -v 2>&1

kill -SIGINT $PID

wait
