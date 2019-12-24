#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <memory.h>
#include <omp.h>
#include "mpi.h"

#define ROW 4
#define COL 5

#define ENCRYPT_KEY 3
#define MAX_LENGTH 255
#define THREADS 2

// Store wireless sensor network node information
typedef struct WSNNode
{
    int row;
    int col;
    int node_number;
    int *adjacents;
    int adjacent_count;
} Node;

// Store adjacent node information
typedef struct AdjacentInfo
{
    int master_node;
    int adjacents[4];
    int random_number;
    int length;
} Adjacent;

void base_station(MPI_Comm master_comm, int cur_procs, int iteration);
void sensor_node(MPI_Comm master_comm, MPI_Comm comm, int cur_procs);
void adjacent_node(MPI_Comm master_comm, MPI_Comm comm, Node sensor_node);

int get_random_process(int pre_proc);
int get_random_number(int min, int max, int process);
Node get_sensor_node(int rank);
int *get_adjacent_nodes(int row, int col);
char *node_to_string(Node node);
char *array_to_string(int *arr, int len);
int is_adjacent(int process, int *adjacents, int len);
char *getLocalTime();
int start_with(char* prefix, char* str);

// ================================================================
// http://www.trytoprogram.com/c-examples/c-program-to-encrypt-and-decrypt-string/
//
// Encrypt and Decrypt message using Caesar Cypher Algorithm and OpenMP
// ================================================================
void encrypt_message(char *send_msg);
void decrypt_message(char *recv_msg);

void openmp_encrypt_message(char *send_msg);
void openmp_decrypt_message(char *recv_msg);

// Write data to file
void write_data(char *message);
