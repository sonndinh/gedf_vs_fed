#!/usr/bin/python
# This code generates task set for GEDF vs. FS experiment. 
# Tasks are independent (i.e., there is no shared resource in this work).

import sys
import string
import os
import math
import random
import copy
from taskgen import StaffordRandFixedSum


# The range of parallelism (C/L) from which ones for the tasks are generated
para_low = 35
para_high = 45

# One second is a billion nanoseconds
nsec_per_sec = 1000000000

# One millisecond is a million nanoseconds
nsec_per_msec = 1000000

# Min and max period in milliseconds
#period_min = 10
#period_max = 1000

# Min and max exponent of period (base 2)
# The obtained period's unit is microsecond
period_min_expo = 13
period_max_expo = 20

# One microsecond is a thousand nanoseconds
nsec_per_usec = 1000

# Number of hyper-period we want the task set to run
num_hyper_period = 100

#excpercent = 1/2.0
excpercent = 0.25 
#excpercent = 0.4

#excpercent = 0.2
choice = [0.5, 0.5, 0.5, 0.5, 0.65, 0.65, 0.65, 0.75, 0.75, 1]
#choice = [0.4,0.4,0.4,0.4,0.5,0.5,0.5,0.7,0.7,1]
#choice = [0.1, 0.15, 0.3, 0.5, 0.75, 1]


# Generate ratio between span and period (span/period)
def excp_generate():
	return excpercent*(random.choice(choice))

# Generate period in nanosecond
def period_generate():
	period_us = int(math.pow(2, random.randint(period_min_expo, period_max_expo)))
	return int(period_us * nsec_per_usec)

# Generate segment length in nanosecond
# We want to generate segment length relative to span
def threadtype_generate(span):
	temp = int(math.floor(random.lognormvariate(0.6, 3)+5))
	#if span < temp*1000:
	#	print "INVALID! Span: ", span, ". temp: ", temp
	#	exit()
	return int(1000 * math.floor(span/(temp*1000)) )

# Supplementary function for sorting the list of segments by lengths
def getKey(item):
	return item[1]


# Return the parallelism of the task in range [low, high). 
# The return value is floating point.
def parallelism_generate(low, high):
	return random.uniform(low, high)


# Basic function to generate a task's parameters: period, work, span.
def parameters_gen_basic(util):
	period = period_generate()
	work = (int) (period * util)
	span = excp_generate() * period

	return period, work, span


# Generate a task's parameters: period, work, span.
# This function is meant to be used to varying parallelism experiments.
# The parallelism of the task is generated uniformly from "para_low" to "para_high".
def parameters_gen_varying_parallelism(util):
	period = period_generate()
	parallelism = parallelism_generate(para_low, para_high)
	work = (int) (period * util)
	span = (int) (work/parallelism)

	return period, work, span


# Generate parameters: period, work, span for task with 
# specified utilization and utilization lost. 
# Note that the utilization lost is calculated through 
# the number of cores allocated to this task.
def parameters_gen_varying_util_lost(util, num_cores):
	period = period_generate()
	work = (int) (period * util)
	ratio = (max((float)(num_cores-1), util) + (float)(num_cores))/2
	span = (int) ((ratio*period - work)/(ratio - 1))

	return period, work, span


# Generate program with the given set of parameters. 
# Output keeps the same, i.e., the program structure:
# list of segments [segid, #strands, segment length].
def program_generate(period, expected_work, expected_span):
	sumtime = 0
	sumwork = 0
	segid = 0
	program = []
	segments = [] # list of segment [segid, segment length]
	
	# First, generate a list of segment lengths that makes up the span
	while sumtime < expected_span:
		seglength = threadtype_generate(expected_span)
		while ((sumtime + seglength) > expected_span and sumtime == 0) or seglength == 0:
			seglength = threadtype_generate(expected_span)
		if (sumtime + seglength) > expected_span:
			seglength = expected_span - sumtime
			#break
		sub = [] # [segid, segment length]
		segid += 1
		sumtime += seglength
		sub.append(segid)
		sub.append(seglength)
		segments.append(sub)

	# Initiate the program structure
	for segment in segments:
		sub = []
		sub.append(segment[0])
		sub.append(1)
		sub.append(segment[1])
		program.append(sub)
		
		# Also initiate the work
		sumwork += segment[1]
	
	# Make a copy of the list of segments
	# Sort it by increasing length of the segments
	segments_copy = copy.copy(segments)
	segments_copy = sorted(segments_copy, key=getKey)
	
	# Then iteratively add strands until it adds up to work (approximately)
	while sumwork < expected_work:
		seg = random.choice(segments)
		if (sumwork + seg[1]) <= expected_work:
			program[seg[0]-1][1] += 1
			sumwork += seg[1]
		else:
			# Find the last strand that can be added to the program
			last_segid = -1
			for segment in segments_copy:
				if (sumwork + segment[1]) <= expected_work:
					last_segid = segment[0]
				else:
					break
			if last_segid != -1:
				program[last_segid-1][1] += 1
				sumwork += program[last_segid-1][2]
			break
	
	#print "Final program: ", program
	#check_work = 0
	#for segment in program:
	#	check_work += segment[1]*segment[2]

	#if check_work != sumwork:
	#	print "Something wrong !"
	#print "Sumwork: ", sumwork, ". Check work: ", check_work

	actual_util = 1.0*sumwork/period
	#print "Util: ", util, ". Actual util: ", actual_util

	if (float)(abs(sumtime - expected_span))/expected_span > 0.01:
		print "Expected span: ", expected_span, ". Generated span: ", sumtime
	if (float)(abs(sumwork - expected_work))/expected_work > 0.01:
		print "Expected work: ", expected_work, ". Generated work: ", sumwork

	required_cores = math.ceil((float)(sumwork - sumtime)/(period - sumtime))
	print "Actual util: ", actual_util, "Required cores by FS: ", required_cores	
	
	return period, program, actual_util

# A function to test how close is the generated utilization to the expected utilization
def test_program_generate():
	count = 0
	for i in range(1, 1000):
		period, work, span = parameters_gen_basic(1.25)
		period, program, actual_util = program_generate(period, work, span)
		if 1.25 - actual_util >= 0.01:
		#if math.sqrt(12) - actual_util >= 0.01:
			count += 1
			print "Actual util: ", actual_util, ". Expected util: ", 1.25 #math.sqrt(12)
	print "Count: ", count

#test_program_generate()

# Use the random fixed sum algorithm to generate utilizations.
# Inputs:
# @n: number of tasks in the task set
# @m: number of cores in the system
# @fraction: ratio of total utilization over number of cores
# @util_min & @util_max: range of each task's utilization
# Return: a list of individual task utilizations
def generate_tasks_utils(n, m, fraction, util_min, util_max):
	lower = util_min
	upper = util_max
	U = fraction * m

	# Generate list of utilizations in [0.0, 1.0]
	x = StaffordRandFixedSum(n, float(U-n*lower)/(upper-lower), 1)

	# Convert them into range [lower, upper]
	X = x*(upper-lower) + lower
	
	return X[0]

# Function to test the utilizations generated by Stafford procedure
def test_tasks_utils():
	for i in range(0, 2):
		utils = generate_tasks_utils(8, 16, 0.75, 1.25, math.sqrt(16))
		for util in utils:
			sys.stdout.write(str(util) + '\t')
		print

#test_tasks_utils()

# Generate task set from a list of generated utilizations for its tasks
def taskset_generate_basic(n, m, fraction, util_min, util_max):
	utils = generate_tasks_utils(n, m, fraction, util_min, util_max)

	# This is a list of hard-coded utilizations for task set of 3 tasks.
	# It is aimed for an experiment of GEDF vs. FS.
	#utils = [2.1, 2.1, 2.1]

	# Only 1 task has the "bad" utilization (2.1)
	#utils = [2.1, 2.5, 1.4]
	
	taskset = []
	actual_total_util = 0
	total_util = 0

	for util in utils:
		period, work, span = parameters_gen_basic(util)
		#period, work, span = parameters_gen_varying_parallelism(util)
		period, program, actual_util = program_generate(period, work, span)
		task = [period, program, actual_util]
		taskset.append(task)

		actual_total_util += actual_util
		total_util += util

	#print "Actual total util: ", actual_total_util, ". Total util: ", total_util
	return taskset

def test_taskset_generate():
	taskset = taskset_generate_basic(6, 16, 0.75, 1.25, 4)
	
#test_taskset_generate()

# Generate task set with a specified total utilization and total utilization lost
def taskset_generate_varying_util_lost(n, m, norm_util, util_min, util_max, norm_util_lost):
	u = m * norm_util
	u_lost = m * norm_util_lost

	tries_count = 0
	cores_to_tasks = []
	while True:
		utils = generate_tasks_utils(n, m, norm_util, util_min, util_max)
		tries_count += 1
		
		total_ceil_util = 0
		for util in utils:
			total_ceil_util += math.ceil(util)
	
		# If the sum of the ceilings of utilizations is bigger than the number 
		# of cores required by Federated Scheduling, generate a new set of utilizations.
		if total_ceil_util > (u + u_lost):
			continue
		
		# Init the number of cores to each task equals to the ceiling of its utilization
		for util in utils:
			cores_to_tasks.append(math.ceil(util))
	
		# The number of cores left after the initialization
		spare_cores = (u + u_lost) - total_ceil_util

		# Allocate the spare cores to the tasks in a random manner
		while (spare_cores > 0):
			task_id = random.randint(1, n)
			cores_to_tasks[task_id - 1] += 1
			spare_cores -= 1
		
		# Now as we found a valid task, stop the while loop
		break

	print "Number of trials to get valid task set: ", tries_count
	print "Utilizations: "
	for util in utils:
		print util, " "

	print "Cores: "
	for cores in cores_to_tasks:
		print cores, " "

	taskset = []
	actual_total_util = 0
	total_util = 0

	i = 0
	for util in utils:
		num_cores = cores_to_tasks[i]
		i += 1
		period, work, span = parameters_gen_varying_util_lost(util, num_cores)

		# Verify the generated parameters
		generated_util = (float)(work)/period
		generated_ratio = (float)(work-span)/(period-span)
		generated_cores = math.ceil(generated_ratio)
		if generated_cores != num_cores:
			print "Task ", i,": Expected util: ", util, ", #cores: ", num_cores, "Calculated util: ", generated_util, \
			    ". Calculated #cores: ", generated_cores

		period, program, actual_util = program_generate(period, work, span)
		task = [period, program, actual_util]
		taskset.append(task)

		actual_total_util += actual_util
		total_util += util

	return taskset

# Return the utilization of a task based on standard deviation
# Task utilization is generated with normal distribution with 
# a fixed mean value (mean of 1.25 and sqrt(m))
def get_task_util(m, std_dev_factor):
	mean = (1.25 + math.sqrt(m))/2
	std_dev = std_dev_factor * m
	while True:
		util = random.normalvariate(mean, std_dev)
		if util >= 1.25 and util <= math.sqrt(m):
			return util
	

def test_util_gen():
	util = get_task_util(12, 0.1)
	print "Generated util: ", util


# Convert a time duration in nanoseconds to a pair (seconds, nanoseconds)
def convert_nsec_to_timespec(length):
	if length > nsec_per_sec:
		len_sec = length/nsec_per_sec
		len_nsec = length - nsec_per_sec*len_sec
	else:
		len_sec = 0
		len_nsec = length

	return len_sec, len_nsec


# Get hyper-period of the task set
# Since periods are multiple of each other by factor of 2,
# the hyper-period is just the maximum period among tasks
def get_taskset_hyperperiod(taskset):
	periods = []
	for task in taskset:
		periods.append(task[0])
	return max(periods)

# Return the number of times a task will be released
def get_num_iterations(task, hyper_period):	
	period = task[0]
	times = hyper_period / period
	return (num_hyper_period * times)


# Return the next number for a rtpt file
def get_rtpt_file_number(directory):
	list_dir = os.listdir(directory)
	count = 0
	for f in list_dir:
		if f.endswith('.rtpt'):
			count += 1
	return (count+1)

# Write the tasks' structures to an .rtpt file.
# No shared resources in this task system.
def write_to_rtpt(taskset, sys_first_core, sys_last_core, directory):
	m = sys_last_core - sys_first_core + 1
	hyper_period = get_taskset_hyperperiod(taskset)
	f_num = get_rtpt_file_number(directory)
	f = open(str(directory)+'/taskset'+str(f_num)+'.rtpt', 'w')
	lines = str(sys_first_core) + ' ' + str(sys_last_core) + '\n'

	for task in taskset:
		num_segments = len(task[1])
		# A line for command line arguments
		line = "synthetic_task " + str(num_segments) + ' '
		work = 0
		span = 0
		for segment in task[1]:
			span += segment[2]
			work += segment[2] * segment[1]
			if segment[2] >= nsec_per_sec:
				len_sec = segment[2]/nsec_per_sec
				len_nsec = segment[2] - nsec_per_sec*len_sec
			else:
				len_sec = 0
				len_nsec = segment[2]

			line += str(segment[1]) + ' ' + str(len_sec) + ' ' + str(len_nsec) + ' '
			
		line += '\n'
		lines += line

		work_sec, work_nsec = convert_nsec_to_timespec(work)
		span_sec, span_nsec = convert_nsec_to_timespec(span)
		period_sec, period_nsec = convert_nsec_to_timespec(task[0])
		num_iters = get_num_iterations(task, hyper_period)

		# A line for timing parameters
		line2 = str(work_sec)+' '+str(work_nsec)+' '+str(span_sec)+' '+str(span_nsec)+' '+\
		    str(period_sec)+' '+str(period_nsec)+' '+str(period_sec)+' '+str(period_nsec)+' 0 0 '+str(num_iters)+'\n'
		lines += line2
			
	f.write(lines)
	return 

# Genereate task set with the parallelism of each task generated from a specific range.
# The range of parallelism is set with "para_low" & "para_high" parameters. 
# Inputs: first core, last core, number of tasks in the generated task set, total utilization.
# This experiment aims to see how different parallelisms of the tasks affect 
# the performance of GEDF and FS.
def main_varying_parallelism():

	if len(sys.argv) != 5:
		print "Usage: ", sys.argv[0], " <sys_first_core> <sys_last_core> <num_tasks> <total_util_frac>"
		exit()
	
	# Read command-line arguments
	sys_first_core = int(sys.argv[1])
	sys_last_core = int(sys.argv[2])
	if sys_last_core < sys_first_core:
		print "Incorrect system core range!"
		exit()
		
	num_tasks = int(sys.argv[3])
	fraction = float(sys.argv[4])

	# Total number of cores
	m = sys_last_core - sys_first_core + 1

	# Total utilization
	u = m * fraction

	# Set the range of utilization for each task
	util_min = 1.25
	util_max = math.sqrt(m)

	# Check the if the number of tasks is valid
	if num_tasks > math.floor(u/util_min) or num_tasks < math.ceil(u/util_max):
		print 'ERROR: Number of tasks must not be too small or too large!'
		exit()

	# Generate a base task set with the total utilization is (fraction * m)
	taskset = taskset_generate_varying_parallelism(num_tasks, m, fraction, util_min, util_max)
	
	# Write the task set to a .rtpt file
	folder = 'data'
	directory = folder + '/core='+str(m)+'n='+str(num_tasks)+'util='+str(fraction)+'para='+str(low)+'_'+str(high)

	if os.path.isdir(directory) != True:
		os.system('mkdir ' + directory)
	write_to_rtpt(taskset, sys_first_core, sys_last_core, directory)


# Generate task set with a specific number of tasks in a task set. 
# The range of parallelism for each generated task is fixed in this type of experiment.
# This experiment is to see how the number of tasks per task set affects 
# the performance of GEDF and FS. 
def main_varying_num_tasks():

	if len(sys.argv) != 5:
		print "Usage: ", sys.argv[0], " <sys_first_core> <sys_last_core> <num_tasks> <total_util_frac>"
		exit()
	
	# Read command-line arguments
	sys_first_core = int(sys.argv[1])
	sys_last_core = int(sys.argv[2])
	if sys_last_core < sys_first_core:
		print "Incorrect system core range!"
		exit()
		
	num_tasks = int(sys.argv[3])
	fraction = float(sys.argv[4])

	# Total number of cores
	m = sys_last_core - sys_first_core + 1

	# Total utilization
	u = m * fraction

	# Set the range of utilization for each individual task.
	util_min = 1.25
	util_max = u

	# Check the if the number of tasks is valid
	if num_tasks > math.floor(u/util_min) or num_tasks < math.ceil(u/util_max):
		print 'ERROR: Number of tasks must not be too small or too large!'
		exit()

	# Generate a base task set with the total utilization is (fraction * m)
	taskset = taskset_generate_basic(num_tasks, m, fraction, util_min, util_max)
	
	# Write the task set to a .rtpt file
	folder = 'data'
	directory = folder + '/core='+str(m)+'n='+str(num_tasks)+'util='+str(fraction)

	if os.path.isdir(directory) != True:
		os.system('mkdir ' + directory)
	write_to_rtpt(taskset, sys_first_core, sys_last_core, directory)


# Generate task set with varying the total utilization lost (w.r.t Federated Scheduling).
# For each task, its utilization lost equals its required cores minus its utilization. 
# For each value of the total utilization lost, it generates tasks so that the individual 
# utilization losts by the tasks sum up to the total utilization lost. 
# Input: 
# @sys_first_core, @sys_last_core: the system first and last core
# @num_tasks: the number of tasks per task set
# @total_util_frac: the normalized total utilization of the task set
# @total_util_lost_frac: the normalized total utilization lost of the task set
# Note that for each task tau_i, (u_i + u^lost_i) = n_i, thus the sum of 
# a task's utilization and its utilizaiton lost is an integer.
def main_varying_util_lost():
	if len(sys.argv) != 6:
		print "Usage: ", sys.argv[0], " <sys_first_core> <sys_last_core> <num_tasks> <total_util_frac> <total_util_lost_frac>"
		exit()
	
	# Read command-line arguments
	sys_first_core = int(sys.argv[1])
	sys_last_core = int(sys.argv[2])
	if sys_last_core < sys_first_core:
		print "Incorrect system core range!"
		exit()
		
	num_tasks = int(sys.argv[3])
	norm_util = float(sys.argv[4])
	norm_util_lost = float(sys.argv[5])

	# Total number of cores
	m = sys_last_core - sys_first_core + 1

	# Total utilization
	u = m * norm_util

	# Total utilization lost
	u_lost = m * norm_util_lost

	# The sum of the total utilization and the total utilization lost must be an integer
	# (i.e., the total cores required by the federated scheduling for this task set)
	if (float(math.ceil(u + u_lost)) - (u + u_lost)) != 0:
		print "Total utilization + total utilization lost must be an integer !!"
		exit()

	# Set the range of utilization for each individual task.
	util_min = 1.25
	util_max = u

	# Check the if the number of tasks is valid
	if num_tasks > math.floor(u/util_min) or num_tasks < math.ceil(u/util_max):
		print 'ERROR: Number of tasks must not be too small or too large!'
		exit()

	# Generate a base task set with the total utilization is (fraction * m)
	taskset = taskset_generate_varying_util_lost(num_tasks, m, norm_util, util_min, util_max, norm_util_lost)

	'''
	# Write the task set to a .rtpt file
	folder = 'data'
	directory = folder + '/core='+str(m)+'n='+str(num_tasks)+'util='+str(norm_util)+'lost='+str(norm_util_lost)

	if os.path.isdir(directory) != True:
		os.system('mkdir ' + directory)
	write_to_rtpt(taskset, sys_first_core, sys_last_core, directory)
	'''


main_varying_util_lost()
