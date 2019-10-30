#!/bin/bash

PROC_NUM=16
TOTAL_UTIL=0.75
NUM_TASKS=5

path='../data/core='${PROC_NUM}'n='${NUM_TASKS}'util='${TOTAL_UTIL}'lost=0.3125'
for i in {1..100}
do
	file=${path}'/taskset'${i}'.rtpt'
	./partition ${file}
done

echo "Finished!"