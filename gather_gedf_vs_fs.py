#!/usr/bin/python
import os
from collections import OrderedDict

# Define parameters of the task sets we want to gather
num_cores = 16
util=0.75

# Parallelisms of the tasks are generated in this range
para_low = 35
para_high = 45

# For fixed number of tasks in each task set
num_tasks = 5

# Number of task sets we want to gather
num_tasksets = 100

# Gather analysis data for either type of locks
# @folder: path to the folder that contains result files.
# This folder must directly contain rtps files under it.
# Return: @number of schedulable task sets
#         @number of valid task sets
def gather_analysis_data(folder):

    # Count number of analytically schedulable task sets
    analysis_count = 0
    
    num_valid_tsets = num_tasksets
    for i in range(1, num_tasksets+1):
        file_path = folder + '/taskset' + str(i) + '.rtps'
        rtps_file = open(file_path, 'r')
        line = rtps_file.readline()
        if int(line) == 2:
            num_valid_tsets -= 1
            continue
        
        if int(line) == 0:
            analysis_count += 1
        
        rtps_file.close()

    return analysis_count, num_valid_tsets

# Gather empirical data for either type of locks.
# @folder: the folder contains outputs for the task sets
# Return: @number of successfully scheduled task sets
#         @number of valid task sets
def gather_exper_data(folder, algo):

    if algo == 'fs':
        tail = ''
    elif algo == 'gedf':
        tail = '_gedf'

    # Count number of successfully scheduled task sets
    exper_count = 0
    num_valid_tsets = num_tasksets
    for i in range(1, num_tasksets+1):
        # Read the schedulability of the experiments
        exper_result_folder = folder + '/taskset' + str(i) + '_output'

        # Track whether the task set is successfully scheduled
        successful = True

        # While reading task output files, track whether the task set is schedulable
        for j in range(1, num_tasks+1):
            task_path = exper_result_folder + '/task' + str(j) + tail + '.txt'

            task_file = open(task_path, 'r')
            first_line = task_file.readline()
            if first_line == "Binding failed !":
                print 'ALERT: Task set ', i, ' failed to bind!'
                successful = False
                num_valid_tasksets -= 1
                task_file.close()
                break

            if first_line:
                fraction = first_line.rsplit(' ', 1)[1]
                num_fails = fraction.rsplit('/', 1)[0]
                if int(num_fails) > 0:
                    successful = False
                    task_file.close()
                    break

        if successful == True:
            exper_count += 1

    return exper_count, num_valid_tsets


# Gather all analysis and empirical data for FIFO, DM-based, optimal locking priority
def gather_all_data():
    #folder = 'data/core='+str(num_cores)+'n='+str(num_tasks)+'util='+str(util)+'para='+str(para_low)+'_'+str(para_high)
    folder = 'data/core='+str(num_cores)+'n='+str(num_tasks)+'util='+str(util)+'lost=0.3125'

    # Collect analysis data for FS
    ana, ana_valid = gather_analysis_data(folder)
        
    # Collect empirical data for FS and GEDF
    fs, fs_valid = gather_exper_data(folder, 'fs')
    gedf, gedf_valid = gather_exper_data(folder, 'gedf')

    # Store the percentage of schedulable task sets
    result_analysis = float(ana)/ana_valid
    
    # Store the percentage of experimentally successful task sets
    result_exper_fs = float(fs)/fs_valid
    result_exper_gedf = float(gedf)/gedf_valid

    return result_analysis, result_exper_fs, result_exper_gedf


def main():
    ana, fs, gedf = gather_all_data()
    print 'Analysis\tFS\tGEDF\n'
    print ana, '\t', fs, '\t', gedf


main()
