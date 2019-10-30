#!/usr/bin/python
from taskget import StaffordRandFixedSum
import sys
import os

# Inputs:
# @n: number of tasks in the task set
# @m: number of cores in the system
# @fraction: normalized utilization
# @lower: lower-bound for the individual utilization
# @upper: upper-bound for the individual utilization
# Return: a list of individual utilizations
def generate_utils(n, m, fraction, lower, upper):
    U = fraction * m
    # Generate list of utilizations in [0.0, 1.0].
    x = StaffordRandFixedSum(n, float(U-n*lower)/(upper-lower), 1)
    # Convert them into the range [lower, upper].
    X = x * (upper - lower) + lower
    return X[0]

def example():
    n = 5
    m = 32
    fraction = 0.5
    util_min = 1.0
    util_max = 4.0
    utils = generate_utils(n, m, fraction, util_min, util_max)
    line = ""
    for util in utils:
        line += str(util) + " "
    print "Utilizations:", line
    
    # Write the utils to a file in the task set folder.
    path = "tmp/taskset"
    try:
        if not os.path.exists(path):
            os.mkdir(path)
    except OSError:
        print "Create folder", path, "failed !!"
    else:
        print "Create folder", path, "successfully !!"
        # Write utils to a file.
        f = open(path+"/utils.txt", 'w+')
        for util in utils:
            f.write(str(util) + "\n")
        f.close()

if __name__ = "__main__":
    example()
