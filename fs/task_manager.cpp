// Each real time task should be compiled as a separate program and include task_manager.cpp and task.h
// in compilation. The task struct declared in task.h must be defined by the real time task.
#define __STDC_FORMAT_MACROS
#include <stdint.h> //For uint64_t
#include <inttypes.h> //For PRIu64
#include <stdlib.h> //For malloc
#include <sched.h>
#include <unistd.h> 
#include <stdio.h>
#include <math.h>
#include <sstream>
#include <signal.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <string>
#include "task.h"
#include "timespec_functions.h"
#include "single_use_barrier.h"


//There are one trillion nanoseconds in a second, or one with nine zeroes
const unsigned nsec_in_sec = 1000000000; 

enum rt_gomp_task_manager_error_codes
{ 
	RT_GOMP_TASK_MANAGER_SUCCESS,
	RT_GOMP_TASK_MANAGER_CORE_BIND_ERROR,
	RT_GOMP_TASK_MANAGER_SET_PRIORITY_ERROR,
	RT_GOMP_TASK_MANAGER_INIT_TASK_ERROR,
	RT_GOMP_TASK_MANAGER_RUN_TASK_ERROR,
	RT_GOMP_TASK_MANAGER_BARRIER_ERROR,
	RT_GOMP_TASK_MANAGER_BAD_DEADLINE_ERROR,
	RT_GOMP_TASK_MANAGER_ARG_PARSE_ERROR,
	RT_GOMP_TASK_MANAGER_ARG_COUNT_ERROR
};


// Task parameters
int priority;
unsigned first_core, last_core;
timespec period, deadline, relative_release;

// Return time in nanosecond
unsigned long long timespec2ns(timespec ts) {
	return (ts.tv_sec*1000000000 + ts.tv_nsec);
}


int main(int argc, char *argv[])
{
	// Process command line arguments	
	const char *task_name = argv[0];
	
	// For FS, there are 13 required arguments passed from clustering_launcher
	// (the same 12 as GEDF and the barrier name)
	const int num_req_args = 13;
	if (argc < num_req_args)
	{
		fprintf(stderr, "ERROR: Too few arguments for task %s\n", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_ARG_COUNT_ERROR;
	}
	
	unsigned num_iters;
	long period_sec, period_ns, deadline_sec, deadline_ns, relative_release_sec, relative_release_ns;
	if (!(
		std::istringstream(argv[1]) >> first_core &&
		std::istringstream(argv[2]) >> last_core &&
		std::istringstream(argv[3]) >> priority &&
		std::istringstream(argv[4]) >> period_sec &&
		std::istringstream(argv[5]) >> period_ns &&
		std::istringstream(argv[6]) >> deadline_sec &&
		std::istringstream(argv[7]) >> deadline_ns &&
		std::istringstream(argv[8]) >> relative_release_sec &&
		std::istringstream(argv[9]) >> relative_release_ns &&
		std::istringstream(argv[10]) >> num_iters
	))
	{
		fprintf(stderr, "ERROR: Cannot parse input argument for task %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_ARG_PARSE_ERROR;
	}
	
	char *barrier_name = argv[11];

	int task_argc = argc - (num_req_args-1);
	char **task_argv = &argv[num_req_args-1];

	period = { period_sec, period_ns };
	deadline = { deadline_sec, deadline_ns };
	relative_release = { relative_release_sec, relative_release_ns };
	
	// Check if the task has a run function
	if (task.run == NULL) {
		fprintf(stderr, "ERROR: Task does not have a run function %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_RUN_TASK_ERROR;
	}

	// Bind the task to the assigned cores
	cpu_set_t mask;
	CPU_ZERO(&mask);
	for (unsigned i = first_core; i <= last_core; ++i) {
		CPU_SET(i, &mask);
	}
	
	int ret_val = sched_setaffinity(getpid(), sizeof(mask), &mask);
	if (ret_val != 0) {
		perror("ERROR: Could not set CPU affinity");

		// SonDN (18 Jan, 2016): comment this out since it causes the group of processes terminate.
		// Instead just terminate this child process (must call await_single_use_barrier first since
		// other children tasks are waiting).
		//	kill(0, SIGTERM);

		// Wait at barrier for the other tasks
		if (await_single_use_barrier(barrier_name) != 0) {
				fprintf(stderr, "ERROR: Barrier error for task %s", task_name);
				kill(0, SIGTERM);
				return RT_GOMP_TASK_MANAGER_BARRIER_ERROR;
		}
		
		// Write to its output file saying that it failed
		fprintf(stdout, "Binding failed !");
		fflush(stdout);

		return RT_GOMP_TASK_MANAGER_CORE_BIND_ERROR;
	}
	
	// Each task uses the highest real-time priority on its dedicated cores
	sched_param sp;
	sp.sched_priority = 97;

	ret_val = sched_setscheduler(getpid(), SCHED_FIFO, &sp);
	if (ret_val != 0)
	{
		perror("ERROR: Could not set process scheduler/priority");
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_SET_PRIORITY_ERROR;
	}
	
	// Fix the number of threads in the team
	omp_set_dynamic(0);

	// Disable nested parallel regions
	omp_set_nested(0);

	// Turn on OMP's greedy scheduler
	omp_set_schedule(omp_sched_dynamic, 1);
	//omp_set_schedule(omp_sched_guided, 1);

	// Set the number of threads to the number of allocated cores
	omp_set_num_threads(omp_get_num_procs());

	//	fprintf(stderr, "==== INFO ====: OpenMP get_num_procs: %d. Number of cores: %d\n", omp_get_num_procs(), last_core-first_core+1);

	omp_sched_t omp_sched;
	int omp_mod;
	omp_get_schedule(&omp_sched, &omp_mod);
	fprintf(stderr, "OMP sched: %u %u\n", omp_sched, omp_mod);
	
	fprintf(stderr, "Initializing task %s\n", task_name);

	// Initialize the task
	if (task.init != NULL)
	{
		ret_val = task.init(task_argc, task_argv);
		if (ret_val != 0)
		{
			fprintf(stderr, "ERROR: Task initialization failed for task %s", task_name);
			kill(0, SIGTERM);
			return RT_GOMP_TASK_MANAGER_INIT_TASK_ERROR;
		}
	}


	//Create storage for per-job timings
	uint64_t *period_timings = (uint64_t*) malloc(num_iters * sizeof(uint64_t));

	if (period_timings == NULL) {
		fprintf(stderr, "WARNING: Allocating memory for per-job execution times failed!\n");
	} else {
		fprintf(stderr, "Allocating memory for per-job execution times success!\n");
	}

	fprintf(stderr, "Task %s reached barrier\n", task_name);
	
	// Wait at barrier for the other tasks
	ret_val = await_single_use_barrier(barrier_name);
	if (ret_val != 0)
	{
		fprintf(stderr, "ERROR: Barrier error for task %s", task_name);
		kill(0, SIGTERM);
		return RT_GOMP_TASK_MANAGER_BARRIER_ERROR;
	}

	// Initialize timing controls
	unsigned deadlines_missed = 0;
	timespec correct_period_start, actual_period_start, period_finish, period_runtime;
	timespec max_period_runtime = {0, 0};
	uint64_t total_nsec = 0;
	get_time(&correct_period_start);
	correct_period_start = correct_period_start + relative_release;

	// After receiving the release signal (release_ts()),
	// Now run the loop for the task's jobs
	for (unsigned i = 0; i < num_iters; i++) {
		// Every threads wait to the next period
		sleep_until_ts(correct_period_start);

		// Record the start time of this job
		get_time(&actual_period_start);

		ret_val = task.run(task_argc, task_argv);

		// Record the finish time of this job
		get_time(&period_finish);

		if (ret_val != 0)
		{
			fprintf(stderr, "ERROR: Task run failed for task %s", task_name);
			return RT_GOMP_TASK_MANAGER_RUN_TASK_ERROR;
		}

		// The time it takes for the job to finish
		ts_diff(actual_period_start, period_finish, period_runtime);

		uint64_t time_in_nsec = period_runtime.tv_nsec + nsec_in_sec * period_runtime.tv_sec;
		if (i != 0) { // abort the first job
			if (period_runtime > deadline) deadlines_missed += 1;
			if (period_runtime > max_period_runtime) max_period_runtime = period_runtime;
			total_nsec += time_in_nsec;
		}

		// Record the time for each job
		period_timings[i] = time_in_nsec;

		// Update the period_start time
		correct_period_start = correct_period_start + period;
	}

	
	// Finalize the task
	if (task.finalize != NULL) 
	{
		ret_val = task.finalize(task_argc, task_argv);
		if (ret_val != 0)
		{
			fprintf(stderr, "WARNING: Task finalization failed for task %s\n", task_name);
		}
	}

	// Write the recorded timings to the output file
	fprintf(stdout,"Deadlines missed for task %s: %d/%d\n", task_name, deadlines_missed, num_iters);
	fprintf(stdout,"Max running time for task %s: %i sec  %lu nsec\n", task_name, (int)max_period_runtime.tv_sec, max_period_runtime.tv_nsec);
	fprintf(stdout,"Avg running time for task %s: %" PRIu64  " nsec\n", task_name, total_nsec/(num_iters-1));

	// SonDN (Jan 31, 2016): write the recorded response times to the file
	for (unsigned i=0; i<num_iters; i++) {
		fprintf(stdout, "%" PRIu64 "\n", period_timings[i]);
	}
	
	// Remember to free allocated memory
	free(period_timings);

	fflush(stdout);
	
	return 0;
}
