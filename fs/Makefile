# Compile binaries for running FS clustering experiments

CC = g++
FLAGS = -Wall -std=c++0x
LIBS = -L. -lrt -lpthread -lm
CLUSTER_PATH = -I../../spinlocks_clustering #-I/export/shakespeare/home/sonndinh/codes/spinlocks_clustering #-I/home/sondn/codes/spinlocks_clustering


all: clustering_launcher_fs synthetic_task

synthetic_task: synthetic_task.cpp ../../spinlocks_clustering/timespec_functions.cpp ../../spinlocks_clustering/single_use_barrier.cpp task_manager.cpp 
	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp ../../spinlocks_clustering/timespec_functions.cpp ../../spinlocks_clustering/single_use_barrier.cpp task_manager.cpp -o synthetic_task $(CLUSTER_PATH) $(LIBS)

clustering_launcher_fs: clustering_launcher.cpp ../../spinlocks_clustering/single_use_barrier.cpp
	$(CC) $(FLAGS) clustering_launcher.cpp ../../spinlocks_clustering/single_use_barrier.cpp -o clustering_launcher_fs $(CLUSTER_PATH) $(LIBS)

clean:
	rm -f *.o *.pyc clustering_launcher_fs synthetic_task