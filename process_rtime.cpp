// This file computes the 99th percentile of the ratio 
// (response time)/(relative deadline) for each task in the set of 
// 100 task sets in an experiment.

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>

using namespace std;

const unsigned long NSEC_IN_SEC = 1000000000;

//const unsigned NUM_CORES = 16;
const unsigned NUM_TASKS = 4;
//const float UTIL = 0.75;
//const unsigned PARA_LOW = 5;
//const unsigned PARA_HIGH = 15;

const unsigned NUM_TASKSETS = 100;

float cal_percentile(vector<float> &response_times);

// For each task, read from the output file for that task
int main(int argc, char **argv) {

	// Read a path to the folder containing results
	if (argc != 3) {
		printf("Usage: Program {path_to_result_files} {output_file_name} !\n");
		return 1;
	}

	string path(argv[1]);

	// One vector to store 99th percentile values for each of GEDF and FS
	vector<float> gedf, fs;
	
	for (unsigned i=1; i<=NUM_TASKSETS; i++) {
		stringstream rtpt_ss;
		rtpt_ss << path << "/taskset" << i << ".rtpt";
		string rtpt_file = rtpt_ss.str();
		
		ifstream rtpt_ifs(rtpt_file.c_str());
		if (!rtpt_ifs.is_open()) {
			fprintf(stderr, "ERROR: Cannot open rtpt file");
			return 1;
		}
		
		string line;
		
		// Abort the first line
		getline(rtpt_ifs, line);

		// Map each task to its deadline (in nanoseconds)
		map<unsigned, unsigned long long> deadlines;

		// Read relative deadlines for the tasks in this task set
		for (unsigned j=1; j<=NUM_TASKS; j++) {
			getline(rtpt_ifs, line); // task structure line
			getline(rtpt_ifs, line); // task parameters line
			
			unsigned long work_s, work_ns, span_s, span_ns, deadline_s, deadline_ns;
			istringstream parameters_stream(line);

			parameters_stream >> work_s >> work_ns >> span_s >> span_ns >> deadline_s >> deadline_ns;
			unsigned long long deadline = NSEC_IN_SEC * deadline_s + deadline_ns;

			// Store relative deadline value for this task
			deadlines[j] = deadline;
		}

		// Now for each task in this task set, calculate its 99th percentile response time
		for (unsigned j=1; j<=NUM_TASKS; j++) {
			// Store normalized response times for the jobs of this task
			vector<float> fs_response_times, gedf_response_times;

			stringstream fs_ss, gedf_ss;
			fs_ss << path << "/taskset" << i << "_output/task" << j << ".txt";
			gedf_ss << path << "/taskset" << i << "_output/task" << j << "_gedf.txt";

			ifstream fs_ifs(fs_ss.str().c_str());
			ifstream gedf_ifs(gedf_ss.str().c_str());
			if (!fs_ifs.is_open() || !gedf_ifs.is_open()) {
				fprintf(stderr, "ERROR: Cannot open result files");
				printf("Taskset: %d, task: %d\n", i, j);
				return 2;
			}
			
			// This task's deadline
			unsigned long long deadline = deadlines[j];

			// Read results for FS first
			// Abort 3 first lines
			for (int i=0; i<3; i++) {
				getline(fs_ifs, line);
			}

			while (getline(fs_ifs, line)) {
				stringstream response_time_ss(line);
				unsigned long long response_time;
				response_time_ss >> response_time;
				fs_response_times.push_back((float)response_time/deadline);
			}

			// Then read results for GEDF. Again, abort the first 3 lines.
			for (int i=0; i<3; i++) {
				getline(gedf_ifs, line);
			}

			while(getline(gedf_ifs, line)) {
				stringstream response_time_ss(line);
				unsigned long long response_time;
				response_time_ss >> response_time;
				gedf_response_times.push_back((float)response_time/deadline);
			}

			// Now compute 99 percentile value for the normalized response time 
			// of both GEDF and FS.
			float fs_percentile = cal_percentile(fs_response_times);
			float gedf_percentile = cal_percentile(gedf_response_times);

			// Store these 2 values of 99 percentile for GEDF and FS
			fs.push_back(fs_percentile);
			gedf.push_back(gedf_percentile);
		}
	}

	// Write all 99 percentile values to a file for each GEDF and FS
	//	string result_file("core=16n=5util=0.75para=5_15_percentiles.dat");
	string result_file(argv[2]);
	ofstream result_ofs(result_file.c_str());

	//	result_ofs << "#Parallelism: [5, 15)\t[15, 25)\t[25, 35)\t[35, 45)\n";
	result_ofs << "#GEDF\tFS\n";
	for (int i=0; i<gedf.size(); i++) {
		result_ofs << gedf[i] << "\t" << fs[i] << "\n";
	}
	result_ofs.close();
}

bool sort_func(float i, float j) {
	return (i<j);
}

// Calculate 99th percentile
float cal_percentile(vector<float> &response_times) {
	sort(response_times.begin(), response_times.end(), sort_func);
	double idx = 0.99 * (int)(response_times.size());
	int index = (int)(ceil(idx));

	if (ceil(idx) == idx) {
		return ((response_times[index-1] + response_times[index])/2);
	} else {
		return response_times[index-1];
	}
}
