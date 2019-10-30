// This file reads an input .rtpt file, performs a schedulability test
// for the task set defined in the file. Then it writes the partition 
// to the output .rtps file (for each task, the partition determines 
// a set of cores it is assigned to).
// NOTE: that this code only works with task sets of synthetic_tasks.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include <cmath>
#include <map>
#include <algorithm>


using namespace std;

const unsigned long kNsecInSec = 1000000000;

// Total number of cores in the system
const unsigned kNumCores = 16;

// Whether we find a FS-valid core partition for task set or not.
enum Partition_Status {
	PARTITION_FOUND = 0, // enough cores to allocate by FS
	HEURISTIC_USED = 1, // must use some heuristics to allocate cores to tasks
	INVALID = 2 // there is no valid partition found for this task set
};


// Structure contains information for each task.
// All timing information is in nanoseconds.
// For core allocation, negative values mean the task is not allocated cores yet.
typedef struct Task {
	unsigned id; // id of the task
	unsigned long work;
	unsigned long span;
	unsigned long period;
	unsigned long deadline;
	unsigned long release;
	unsigned required_cores; // number of required cores by federated scheduling
	int first_core; // first core currently assigned to the task
	int last_core;  // last core currently assigned to the task
	unsigned min_cores; // minimum number of cores can be possibly assigned to this task, floor(C/T)
} Task;


// Convert a pair of <seconds, nanoseconds> to nanoseconds
unsigned long convert2nsec(unsigned sec, unsigned long nsec) {
	return (sec * kNsecInSec + nsec);
}

// Calculated the required number of cores for a task by federated scheduling
unsigned fs_required_cores(Task task) {
	unsigned long work = task.work;
	unsigned long span = task.span;
	unsigned long deadline = task.deadline;

	unsigned cores = ceil(((float)(work - span))/(deadline - span));
	return cores;
}

// Information for the task set is stored here
typedef struct TaskSet {
	enum Partition_Status status;
	unsigned total_required_cores; // total number of cores required by FS
	map<unsigned, Task> taskset; // A map from task id to its structure
} TaskSet;


// Track the number of allocated cores for each task
typedef struct Allocated {
	unsigned id; // task id
	unsigned allocated_cores; // already allocated cores for each task
} Allocated;


// Track the number of additional cores each task needs
typedef struct Slack {
	unsigned id; // id of the corresponding task
	unsigned needed_cores; // the number of additional cores it needs
} Slack;

// Track the gap between n_i and (C_i-L_i)/(D_i-L_i) for the tasks
typedef struct Gap {
	unsigned id; // id of the corresponding task
	float gap; // the gap of this task
} Gap;


// Function to sort tasks' slacks in decreasing order
bool sort_slacks(Slack first, Slack second) {
	return (first.needed_cores >= second.needed_cores);
}

// Function to sort tasks' gap values in increasing order
bool sort_gaps(Gap first, Gap second) {
	return (first.gap <= second.gap);
}

// This function does the core partitioning for the task set
void partition(TaskSet &ts) {

	// Calculate the total number of cores required by federated scheduling
	unsigned total_cores = 0;
	map<unsigned, Task>::iterator it;
	for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
		it->second.required_cores = fs_required_cores(it->second);
		total_cores += it->second.required_cores;
	}
	
	ts.total_required_cores = total_cores;

	// Enough (or more) cores to allocate by FS.
	if (ts.total_required_cores <= kNumCores) {
		ts.status = PARTITION_FOUND;
		
		/*
		unsigned next_core = 0;
		map<unsigned, Task>::iterator it;
		for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
			unsigned required_cores = it->second.required_cores;
			it->second.first_core = next_core;
			it->second.last_core = next_core + required_cores - 1;
			next_core = next_core + required_cores;
		}

		return;
		*/

		// Assign the spare cores to the tasks.
		// Sort the tasks in increasing order of gap between its n_i and (C_i-L_i)/(D_i-L_i).
		// Then assigning the spare cores in that order. For example, task with 
		// a gap of 0.1 is preferred to receive a core than task with a gap of 0.9.
		
		// A vector of the gaps for the tasks
		vector<Gap> gaps;
		
		for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
			Task &task = it->second;
			unsigned n_i = task.required_cores;
			float ratio = (float)(task.work - task.span)/(task.deadline - task.span);
			Gap gap;
			gap.id = it->first;
			gap.gap = (float)n_i - ratio;
			gaps.push_back(gap);
		}

		// Sort the tasks in increasing order of their gaps
		sort(gaps.begin(), gaps.end(), sort_gaps);

		// The number of spare cores
		unsigned spare_cores = kNumCores - ts.total_required_cores;

		// Store the number of cores allocated to each tasks
		map<unsigned, unsigned> allocated_cores;
		for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
			unsigned task_id = it->first;
			allocated_cores[task_id] = it->second.required_cores;
		}

		// Go through the list of task in increasing order and 
		// assign core one-by-one.
		unsigned num_tasks = ts.taskset.size();
		unsigned idx = 0;
		while (spare_cores > 0) {
			unsigned task_id = gaps[idx % num_tasks].id;
			allocated_cores[task_id] += 1;
			idx++;
			spare_cores--;
		}

		// Now set the first core and last core for each task
		unsigned next_core = 0;
		for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
			unsigned id = it->second.id;
			unsigned assigned_cores = allocated_cores[id];
			it->second.first_core = next_core;
			it->second.last_core = next_core + assigned_cores - 1;
			next_core += assigned_cores;
		}
		
		return;
	}

	// If there are not enough cores to allocate by FS
	// Calculate the total number of minimum cores for all tasks
	unsigned total_min_cores = 0;
	for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
		unsigned long work = it->second.work;
		unsigned long period = it->second.deadline; // assuming implicit deadline task set
		it->second.min_cores = floor((float)work/period); // take floor of the task's utilization

		total_min_cores += it->second.min_cores;
	}

	if (total_min_cores <= kNumCores) {
		// There are enough or more cores than the total minimum cores of all tasks
		ts.status = HEURISTIC_USED;

		unsigned spare_cores = kNumCores - total_min_cores;
		
		// Track the number of cores allocated to each task 
		// key: task id. value: number of cores
		map<unsigned, unsigned> allocated;

		// Store the number of additional cores each task needs
		vector<Slack> slacks;
		
		for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
			// Record the cores already allocated to each task
			allocated[it->second.id] = it->second.min_cores;

			Slack slack;
			slack.id = it->second.id;
			slack.needed_cores = it->second.required_cores - it->second.min_cores;
			slacks.push_back(slack);
		}

		// Sort the tasks by decreasing number of additional needed cores
		sort(slacks.begin(), slacks.end(), sort_slacks);
		
		// Distribute spare cores to the tasks
		while (spare_cores > 0) {
			for (unsigned i = 0; i<slacks.size(); i++) {
				if (spare_cores <= 0) { 
					// No more spare core, we're done
					break;
				}

				unsigned task_id = slacks[i].id;
				if (allocated[task_id] >= ts.taskset[task_id].required_cores) {
					// This task is already allocated required cores, move on
					continue;
				}

				// Otherwise, assign it 1 more core
				slacks[i].needed_cores -= 1;
				allocated[task_id] += 1;
				spare_cores -= 1;
			}
		}

		// Now write the allocation to the tasks
		unsigned next_core = 0;
		for (it = ts.taskset.begin(); it != ts.taskset.end(); it++) {
			unsigned id = it->first;
			unsigned alloc_cores = allocated[id];
			it->second.first_core = next_core;
			it->second.last_core = next_core + alloc_cores - 1;
			next_core += alloc_cores;
		}
		return;

	} else {
		// Not enough cores even for minimum cores for each task.
		// Since the minimum core for each task is basically equal to 
		// its utilization (more exactly, less than or equal to), 
		// this case means the total utilization of the task set is 
		// larger than the number of cores in the system.
		// So we just return and ignore this task set and replace with another.
		ts.status = INVALID;
		cout << "ERROR: Task set is too big to run on the system!!!" << endl;
		return;
	}
}

// Write to rtps file
void write_rtps(TaskSet &ts, string rtpt_file_name, vector<string> lines) {

	// Open file rtps
	string rtps_file_name = string(rtpt_file_name, 0, strlen(rtpt_file_name.c_str()) - 1);
	rtps_file_name += "s";
	std::ofstream ofs(rtps_file_name.c_str());
	if (!ofs.is_open()) {
		cerr << "ERROR: Cannot open rtps file to write" << endl;
		return;		
	}

	// Write to it
	ofs << ts.status << "\n";
	ofs << lines[0] << "\n";

	unsigned num_tasks = ts.taskset.size();
	for (unsigned i=0; i<num_tasks; i++) {
		// Write the task arguments
		ofs << lines[1+2*i].c_str() << "\n";

		// Write the task timing parameters
		ofs << lines[2+2*i].c_str() << "\n";

		// Write the task's partition
		// We do not care about the priority values; just set it to 97 for all tasks
		ofs << ts.taskset[i+1].first_core << " " << ts.taskset[i+1].last_core << " 97\n";
	}

	ofs.close();
}

int main(int argc, char *argv[]) {

	if (argc != 2) {
		cout << "Usage: " << argv[0] << " <path_to_rtpt_file>" << endl;
		return -1;
	}

	TaskSet ts;

	ifstream ifs(argv[1]);
	if (!ifs.is_open()) {
		cerr << "ERROR: Cannot open rtpt file" << endl;
		return -1;
	}

	// Calculate the number of tasks
    unsigned num_lines = 0;
	vector<string> lines;
	string line;
	while (getline(ifs, line)) { 
		num_lines += 1; 
		lines.push_back(line);
	}

	unsigned num_tasks = (num_lines-1)/2;
	if ( !(num_lines > 1 && ((num_lines-1) % 2) == 0) ) {
		cerr << "ERROR: Incorrect number of lines" << endl;
		return -1;
	}

	// Go to the beginning of the file
	ifs.clear();
	ifs.seekg(0, ios::beg);

	// Read the system core range
	string system_cores_line;
	if (!getline(ifs, system_cores_line)) {
		cerr << "ERROR: Cannot read the system cores line" << endl;
		return -1;
	}

	istringstream system_cores_stream(system_cores_line);
	unsigned system_first_core, system_last_core;
	if ( !(system_cores_stream >> system_first_core &&
		   system_cores_stream >> system_last_core) ) {
		cerr << "ERROR: Cannot read system core range" << endl;
		return -1;
	}
	
	// The total number of processors in the system (i.e., m)
	unsigned num_cores = system_last_core - system_first_core + 1;

	string task_param_line, task_timing_line;
	for (unsigned i=1; i<=num_tasks; i++) {
		if ( getline(ifs, task_param_line) &&
			 getline(ifs, task_timing_line) ) {
			
			// Read each task's timing parameters
			istringstream task_timing_stream(task_timing_line);
			unsigned work_sec, span_sec, period_sec, deadline_sec, release_sec;
			unsigned long work_ns, span_ns, period_ns, deadline_ns, release_ns;
			unsigned num_iters;
			if ( !(task_timing_stream >> work_sec &&
				   task_timing_stream >> work_ns &&
				   task_timing_stream >> span_sec &&
				   task_timing_stream >> span_ns &&
				   task_timing_stream >> period_sec &&
				   task_timing_stream >> period_ns &&
				   task_timing_stream >> deadline_sec &&
				   task_timing_stream >> deadline_ns &&
				   task_timing_stream >> release_sec &&
				   task_timing_stream >> release_ns &&
				   task_timing_stream >> num_iters) ) {
				
				cerr << "ERROR: Task timing parameter improperly provided" << endl;
				return -1;
			}
			
			// Create a Task structure for this task
			Task task;
			task.id = i;
			task.work = convert2nsec(work_sec, work_ns);
			task.span = convert2nsec(span_sec, span_ns);
			task.period = convert2nsec(period_sec, period_ns);
			task.deadline = convert2nsec(deadline_sec, deadline_ns);
			task.release = convert2nsec(release_sec, release_ns);

			task.first_core = -1;
			task.last_core = -1;

			// Add this task to the task set
			ts.taskset.insert(std::pair<unsigned, Task> (i, task));
			
		} else {
			cerr << "ERROR: Cannot read task parameters" << endl;
			return -1;
		}
	}
	ifs.close();

	// Partition cores
	partition(ts);

	// Write results to a rtps file
	write_rtps(ts, string(argv[1]), lines);
	
	return 0;
}
