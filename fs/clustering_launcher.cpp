// Argument: the name of the taskset/schedule file:

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <vector>
#include <string>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include "single_use_barrier.h"

enum rt_gomp_clustering_launcher_error_codes
{ 
	RT_GOMP_CLUSTERING_LAUNCHER_SUCCESS,
	RT_GOMP_CLUSTERING_LAUNCHER_FILE_OPEN_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_UNSCHEDULABLE_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_BARRIER_INITIALIZATION_ERROR,
	RT_GOMP_CLUSTERING_LAUNCHER_ARGUMENT_ERROR
};

// Son (16 July, 2017) Program usage:
// ./clustering_launcher_wlocks <full_path_to_rtps_file> [cluster_id]
// The second argument is the path to the .rtps file, without ".rtps" trailer.
// For example: ./clustering_launcher_wlocks tasksets/taskset1_fifo
// will read file taskset1_fifo.rtps in folder tasksets, and store the output of the task set
// to folder tasksets/taskset1_output.
// The third argument (optional) is the id of the cluster. This is used in case we want to 
// run multiple cluster simultaneously, for instance, 3 clusters for 12-core experiments. 
// Each of the simultaneous clusters must have a distinguished id since it is used to 
// create shared memory objects exclusively accessed by the tasks in this cluster. 

int main(int argc, char *argv[])
{
	// Define the total number of timing parameters that should appear on the second line for each task
	const unsigned num_timing_params = 11;
	
	// Define the number of timing parameters to skip on the second line for each task
	const unsigned num_skipped_timing_params = 4;
	
	// Define the number of partition parameters that should appear on the third line for each task
	const unsigned num_partition_params = 3;
	
	// Define the name of the barrier used for synchronizing tasks after creation
	std::string barrier_name = "/RT_GOMP_CLUSTERING_BARRIER";

	// Verify the number of arguments
	// First argument (mandatory): path to a rtps file without the .rtps extension
	// Second argument (optional): the cluster number of this cluster. This is used 
	// to create more than 1 clusters running simultaneously in order to speedup the 
	// experiments. For instance, we can create 3 simultaneous clusters, each of size 12 cores.
	// Each cluster needs its own shared memory objects for sharing between its tasks. 
	// When running experiments, have to bind each cluster to a separate set of cores.
	if (argc != 2 && argc != 3)
	{
		fprintf(stderr, "Usage: Program path_to_rtps_wo_extension [cluster_number]\n");
		return RT_GOMP_CLUSTERING_LAUNCHER_ARGUMENT_ERROR;
	}
	
	// Append the third argument to the names of the shared memory objects
	if (argc == 3) {
		barrier_name += argv[2];
	}
	
	// Determine the schedule (.rtps) filenames from the program argument
	std::string schedule_filename(argv[1]);
	schedule_filename += ".rtps";

	// Open the schedule (.rtps) file
	std::ifstream ifs(schedule_filename.c_str());
	if (!ifs.is_open())
	{
		fprintf(stderr, "ERROR: Cannot open schedule file");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_OPEN_ERROR;
	}
	
	// Count the number of tasks
	unsigned num_lines = 0;
	std::string line;
	while (getline(ifs, line)) { num_lines += 1; }
	unsigned num_tasks = (num_lines - 2) / 3;
	if (!(num_lines > 2 && (num_lines - 2) % 3 == 0))
	{
		fprintf(stderr, "ERROR: Invalid number of lines in schedule file");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}
	
	// Seek back to the beginning of the file
	ifs.clear();
	ifs.seekg(0, std::ios::beg);
	
	// Check if the taskset is schedulable.
	// If the value of schedulability line is 2, we do NOT run this task set, 
	// since it means the total utilization is larger than the total cores in the system.
	std::string schedulability_line;
	if (std::getline(ifs, schedulability_line))	{
		unsigned schedulability;
		std::istringstream schedulability_stream(schedulability_line);
		if (schedulability_stream >> schedulability) {
			if (schedulability == 0) {
				fprintf(stderr, "Taskset is schedulable with FS: %s\n", argv[1]);
			} else if (schedulability == 1) {
				fprintf(stderr, "WARNING: Taskset may not be schedulable with FS: %s\n", argv[1]);
			} else {
				fprintf(stderr, "WARNING: Taskset NOT schedulable with FS: %s", argv[1]);
				return RT_GOMP_CLUSTERING_LAUNCHER_UNSCHEDULABLE_ERROR;
			}
		} else {
			fprintf(stderr, "ERROR: Schedulability for FS improperly specified");
			return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
		}
	} else {
		fprintf(stderr, "ERROR: Schedulability improperly specified");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}
	
	// Extract the core range line from the file; currently not used
	std::string core_range_line;
	if (!std::getline(ifs, core_range_line))
	{
		fprintf(stderr, "ERROR: Missing system first and last cores line");
		return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
	}

	// Initialize a barrier to synchronize the tasks after creation
	//	printf("Number of tasks: %u\n", num_tasks);

	int ret_val = init_single_use_barrier(barrier_name.c_str(), num_tasks);
	if (ret_val != 0)
	{
		fprintf(stderr, "ERROR: Failed to initialize barrier");
		return RT_GOMP_CLUSTERING_LAUNCHER_BARRIER_INITIALIZATION_ERROR;
	}
	
	// Iterate over the tasks and fork and execv each one
	std::string task_command_line, task_timing_line, task_partition_line;
	for (unsigned t = 1; t <= num_tasks; ++t)
	{
		if (std::getline(ifs, task_command_line) && 
		    std::getline(ifs, task_timing_line) && 
		    std::getline(ifs, task_partition_line)) {

			std::istringstream task_command_stream(task_command_line);
			std::istringstream task_timing_stream(task_timing_line);
			std::istringstream task_partition_stream(task_partition_line);
			
			// Add arguments to this vector of strings. This vector will be transformed into
			// a vector of char * before the call to execv by calling c_str() on each string,
			// but storing the strings in a vector is necessary to ensure that the arguments
			// have different memory addresses. If the char * vector is created directly, by
			// reading the arguments into a string and and adding the c_str() to a vector, 
			// then each new argument could overwrite the previous argument since they might
			// be using the same memory address. Using a vector of strings ensures that each
			// argument is copied to its own memory before the next argument is read.
			std::vector<std::string> task_manager_argvector;
			
			// Add the task program name to the argument vector
			std::string program_name;
			if (task_command_stream >> program_name) {
				task_manager_argvector.push_back(program_name);
			} else {
				fprintf(stderr, "ERROR: Program name not provided for task");
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
			}
			
			// Add the partition parameters to the argument vector
			std::string partition_param;
			for (unsigned i = 0; i < num_partition_params; ++i) {
				if (task_partition_stream >> partition_param) {
					task_manager_argvector.push_back(partition_param);
				} else {
					fprintf(stderr, "ERROR: Too few partition parameters were provided for task %s", program_name.c_str());
					kill(0, SIGTERM);
					return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
				}
			}
			
			// Check for extra partition parameters
			if (task_partition_stream >> partition_param) {
				fprintf(stderr, "ERROR: Too many partition parameters were provided for task %s", program_name.c_str());
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
			}
			
			// Skip the first few timing parameters that were only needed by the scheduler
			std::string timing_param;
			for (unsigned i = 0; i < num_skipped_timing_params; ++i) {
				if (!(task_timing_stream >> timing_param)) {
					fprintf(stderr, "ERROR: Too few timing parameters were provided for task %s", program_name.c_str());
					kill(0, SIGTERM);
					return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
				}
			}
			
			// Add the timing parameters to the argument vector
			for (unsigned i = num_skipped_timing_params; i < num_timing_params; ++i) {
				if (task_timing_stream >> timing_param) {
					task_manager_argvector.push_back(timing_param);
				} else {
					fprintf(stderr, "ERROR: Too few timing parameters were provided for task %s", program_name.c_str());
					kill(0, SIGTERM);
					return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
				}
			}
			
			// Check for extra timing parameters
			if (task_timing_stream >> timing_param) {
				fprintf(stderr, "ERROR: Too many timing parameters were provided for task %s", program_name.c_str());
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
			}
			
			// Add the barrier name to the argument vector
			task_manager_argvector.push_back(barrier_name);

			// Add the task arguments to the argument vector
			task_manager_argvector.push_back(program_name);
			
			std::string task_arg;
			while (task_command_stream >> task_arg) {
				task_manager_argvector.push_back(task_arg);
			}
			
			// Create a vector of char * arguments from the vector of string arguments
			std::vector<const char *> task_manager_argv;
			for (std::vector<std::string>::iterator i = task_manager_argvector.begin(); i != task_manager_argvector.end(); ++i) {
				task_manager_argv.push_back(i->c_str());
			}

			// NULL terminate the argument vector
			task_manager_argv.push_back(NULL);
			
			fprintf(stderr, "Forking and execv-ing task %s\n", program_name.c_str());
			
			// Fork and execv the task program
			pid_t pid = fork();
			if (pid == 0) {
			    // Redirect STDOUT to a file if specified
				std::ostringstream log_file;
				std::string in_file(argv[1]);
				std::string out_folder = in_file + "_output";

				// Create a file to record the running results for each task
				log_file << out_folder << "/" << "task" << t << ".txt";
				int fd = open(log_file.str().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
				if (fd != -1) {
					dup2(fd, STDOUT_FILENO);
				} else {
					perror("Redirecting STDOUT failed.");
				}
                
				// Const cast is necessary for type compatibility. Since the strings are
				// not shared, there is no danger in removing the const modifier.
				execv(program_name.c_str(), const_cast<char **>(&task_manager_argv[0]));
				
				// Error if execv returns
				perror("Execv-ing a new task failed");
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR;
			} else if (pid == -1) {
				perror("Forking a new process for task failed");
				kill(0, SIGTERM);
				return RT_GOMP_CLUSTERING_LAUNCHER_FORK_EXECV_ERROR;
			}	
		} else {
			fprintf(stderr, "ERROR: Provide three lines for each task in the schedule (.rtps) file");
			kill(0, SIGTERM);
			return RT_GOMP_CLUSTERING_LAUNCHER_FILE_PARSE_ERROR;
		}
	}
	
	// Close the file
	ifs.close();
	
	fprintf(stderr, "All tasks started\n");

	// Wait until all child processes have terminated
	//	while (!(wait(NULL) == -1 && errno == ECHILD));
	pid_t pid;
	int status;

	while((pid = wait(&status))) {
		if (pid != -1) {
			printf("Child PID: %d. Terminate normally? %d. Terminate by signal? %d\n", pid, WIFEXITED(status), WIFSIGNALED(status));

			if (WIFEXITED(status)) {
				printf("Exit status: %d\n", WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				printf("Terminating signal sent: %d\n", WTERMSIG(status));
			}
		} else {
			if (errno == ECHILD) break;
		}
	}


	fprintf(stderr, "All tasks finished\n");
	return 0;
}
