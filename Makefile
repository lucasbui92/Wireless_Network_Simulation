ALL: wireless_detection

#This compiles for Ubuntu 14.04 (gcc version 7.4.0)
#In case of error C99 or with gcc 4.8.0, add "-std=c99" at the end of mpicc
assignment2: wireless_detection.c
	mpicc -fopenmp wireless_detection.c -o wireless_detection

#Change number 3 to any number you desire for the number of iterations
run:
	mpirun -np 21 wireless_detection 3
