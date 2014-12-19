#!/bin/bash

EML_FILE=var/loadtest.eml

PARALLEL_COUNT=$1
COUNT=$2

for i in $(seq 1 $PARALLEL_COUNT); do
    test/loadtest_request.py $COUNT < $EML_FILE &
done

for i in $(seq 1 $PARALLEL_COUNT); do
    test/loadtest_session.py $COUNT < $EML_FILE &
done

wait
