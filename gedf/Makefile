# Compile binaries for running Litmus's GEDF experiments

CC = /usr/local/gcc5/bin/g++
FLAGS = -Wall #-std=c++0x
LIBS = -L. -lrt -lpthread -lm
#LITMUS_INC_PATH = -I/home/sondn/litmus-rt/liblitmus/include -I/home/sondn/litmus-rt/liblitmus/arch/x86/include
#LITMUS_LIB_PATH = -L/home/sondn/litmus-rt/liblitmus
#CLUSTER_PATH = -I/home/sondn/codes/spinlocks_clustering
LITMUS_INC_PATH = -I../../../litmus-rt/liblitmus/include -I../../../litmus-rt/liblitmus/arch/x86/include
LITMUS_LIB_PATH = -L../../../litmus-rt/liblitmus
CLUSTER_PATH = -I../../spinlocks_clustering

all: clustering_launcher_gedf synthetic_task

synthetic_task: synthetic_task.cpp ../../spinlocks_clustering/timespec_functions.cpp task_manager.cpp 
	$(CC) $(FLAGS) -fopenmp synthetic_task.cpp ../../spinlocks_clustering/timespec_functions.cpp task_manager.cpp -o synthetic_task $(LITMUS_INC_PATH) $(LITMUS_LIB_PATH) $(CLUSTER_PATH) $(LIBS) -llitmus

clustering_launcher_gedf: clustering_launcher.cpp
	$(CC) $(FLAGS) -fopenmp clustering_launcher.cpp -o clustering_launcher_gedf ${LITMUS_INC_PATH} ${LITMUS_LIB_PATH} $(LIBS) -llitmus

clean:
	rm -f *.o *.pyc clustering_launcher_gedf synthetic_task