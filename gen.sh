#!/bin/bash

FIRST_CORE=0
LAST_CORE=15
TOTAL_UTIL=0.75

for num_tasks in {1..5}
do
	for i in {1..100}
	do
		/usr/bin/python taskset_generate.py ${FIRST_CORE} ${LAST_CORE} ${num_tasks} ${TOTAL_UTIL}
	done
done

echo "Finished!"