#!/bin/bash

FIRST_CORE=0
LAST_CORE=15
TOTAL_UTIL=0.75
TOTAL_UTIL_LOST=0.3125

for num_tasks in 5
do
	for i in {1..100}
	do
		/usr/bin/python taskset_generate.py ${FIRST_CORE} ${LAST_CORE} ${num_tasks} ${TOTAL_UTIL} ${TOTAL_UTIL_LOST}
	done
done

echo "Finished!"