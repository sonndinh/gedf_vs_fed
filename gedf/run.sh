#!/bin/bash

PROC_NUM=16
TOTAL_UTIL=0.75

for NUM_TASKS in 5
do
	path='../data/core='${PROC_NUM}'n='${NUM_TASKS}'util='${TOTAL_UTIL}'lost=0.3125'
	for i in {1..100}
	do
		output_folder=${path}'/taskset'${i}'_output'
		mkdir ${output_folder}
		
		rtps_file=${path}'/taskset'${i}
		
		./clustering_launcher_gedf ${rtps_file}
		sleep 2s
	done
done

echo "GEDF cluster finished!"