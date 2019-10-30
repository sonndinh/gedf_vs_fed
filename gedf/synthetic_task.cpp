// The argument list of a synthetic task includes:
// program-name num-segments {[num-strands len-sec len-ns] ...}

#include <omp.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "task.h"
#include "timespec_functions.h"

using namespace std;

extern int priority;
extern unsigned first_core, last_core;

const unsigned long kNanosecInSec = 1000000000;

typedef struct {
	unsigned num_strands;
	unsigned long len_sec;
	unsigned long len_ns;
	timespec len;
} Segment;

typedef struct {
	unsigned num_segments;
	Segment *segments;
} Program;


// Store structure of the task
// - A task consists of a list of segments (1st dimension)
Program program;

// Convert length in nanosecond to timespec
timespec ns_to_timespec(unsigned long len) {
	unsigned len_sec;
	unsigned len_ns;
	if (len >= kNanosecInSec) {
		len_sec = len/kNanosecInSec;
		len_ns = len - len_sec*kNanosecInSec;
	} else {
		len_sec = 0;
		len_ns = len;
	}

	timespec ret = {len_sec, len_ns};
	return ret;
}

// Initialize data structure for lock objects & program structure
int init(int argc, char* argv[]) {

    if (argc <= 1)
    {
        fprintf(stderr, "ERROR: Two few arguments");
	    return -1;
    }
	
    unsigned num_segments;
    if (!(std::istringstream(argv[1]) >> num_segments))
    {
        fprintf(stderr, "ERROR: Cannot parse input argument");
        return -1;
    }

	// Allocate memory for storing pointers of segments
	program.num_segments = num_segments;
	program.segments = (Segment*) malloc(num_segments * sizeof(Segment));
	if (program.segments == NULL) {
		fprintf(stderr, "ERROR: Cannot allocate memory for segments");
		return -1;
	}

	// Keep track of current argument index
	unsigned arg_idx = 2;
	for (unsigned i=0; i<num_segments; i++) {
		unsigned num_strands;
		if (!(std::istringstream(argv[arg_idx]) >> num_strands)) {
			fprintf(stderr, "ERROR: Cannot read number of strands");
			return -1;
		}
		arg_idx++;
		
		// Allocate memory for storing strands of this segment, NULL at the end
		Segment *current_segment = &(program.segments[i]);
		current_segment->num_strands = num_strands;

		unsigned long len_sec;
		unsigned long len_ns;
		if (!(std::istringstream(argv[arg_idx]) >> len_sec &&
			  std::istringstream(argv[arg_idx+1]) >> len_ns)) {
			fprintf(stderr, "ERROR: Cannot parse input argument");
			return -1;
		}
		current_segment->len_sec = len_sec;
		current_segment->len_ns = len_ns;
		current_segment->len = {len_sec, len_ns};
		arg_idx += 2;
	}

	return 0;
}

int run(int argc, char *argv[])
{
	unsigned num_segments = program.num_segments;
	for (unsigned i=0; i<num_segments; i++) {
		Segment *segment = &(program.segments[i]);
		unsigned num_strands = segment->num_strands;

		#pragma omp parallel for schedule(runtime)
		for (unsigned j=0; j<num_strands; j++) {
			busy_work(segment->len);
		}

	}
	
	return 0;
}

int finalize(int argc, char* argv[]) {

	Segment *segments = program.segments;

	free(segments);
	program.segments = NULL;

	return 0;
}

task_t task = {init, run, finalize};
