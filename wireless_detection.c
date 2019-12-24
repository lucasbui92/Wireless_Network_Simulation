#include "wireless_detection.h"

static const char OUT_FILE_NAME[] = "output.txt";

// ===================================================================
// MAIN FUNCTION
// -------------------------------------------------------------------
// 1. Base-Station is process at rank = 0
// 2. Sensor and its adjacent nodes start from rank = 1 to 20 (ROW * COL)
//
// ----------------------------------
// |  1  |  2   |  3  |  4   |  5   |
// ----------------------------------
// |  6  |  7   |  8  |  9   |  10  |
// ----------------------------------
// |  11 |  12  |  13 |  14  |  15  |
// ----------------------------------
// |  16 |  17  |  18 |  19  |  20  |
// ----------------------------------
//
// ===================================================================
int main(int argc, char *argv[])
{
    int rank, size;
    MPI_Comm new_comm;

    MPI_Init(&argc, &argv);

    int current_process = 0;
    int flag = 0;
    int iteration_num = 10;
    if (argc == 2)
    {
        iteration_num = atoi(argv[1]);
    }

    // File open
    fopen(OUT_FILE_NAME, "w");

    int iteration = 1;
    while (iteration_num > 0)
    {
        MPI_Initialized(&flag);

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_split(MPI_COMM_WORLD, rank == 0, rank, &new_comm);

        if (rank == 0)
        {
            current_process = get_random_process(current_process);
        }

        // transfer value of current process to all other processes
        MPI_Bcast(&current_process, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // check if current running node at corner of matrix, then not trigger it
        Node running_node = get_sensor_node(current_process);
        if (running_node.adjacent_count < 3)
        {
            // printf("goto finish_iteration\n");
            goto finish_iteration;
        }

        if (rank == 0)
        {
            base_station(MPI_COMM_WORLD, current_process, iteration);
        }
        else
        {
            sensor_node(MPI_COMM_WORLD, new_comm, current_process);
        }

finish_iteration:
        MPI_Finalized(&flag);

        // decrease the iterater
        iteration_num -= 1;
        iteration += 1;
    }

    MPI_Finalize();

    return 0;
}

// ===================================================================
// BASE-STATION TRIGGER FUNCTION
// ===================================================================
void base_station(MPI_Comm master_comm, int cur_procs, int iteration)
{
    char buff[MAX_LENGTH];
    MPI_Status status;

    // Get sensor node
    Node node = get_sensor_node(cur_procs);

    // Open file to append result
    sprintf(buff, "%s", node_to_string(node));
    // printf("%s\n", buff);
    write_data("--------------------------------------------------");
    write_data(buff);
    write_data("--------------------------------------------------");

    int counter = 0;
    do
    {
        char recv_msg[100];
        MPI_Recv(&recv_msg, MAX_LENGTH, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, master_comm, &status);

        if(start_with("finish_iterator", recv_msg) == 1)
        {
            break;
        }
        else
        {
            double start_time = MPI_Wtime();

            // Decrypt message after receiving from sensor-node
            openmp_decrypt_message(recv_msg);

            double end_time = MPI_Wtime();

            // The time taken to Encrypt the message
            sprintf(buff, "[RECV] [Decrypt] The time taken to Decrypt message after receiving: %lf", end_time - start_time);
            write_data(buff);

            // Base-station writes decryption message to file
            sprintf(buff, "[RECV] [Base-Station] %s\n", recv_msg);
            write_data(buff);

            counter += 1;
        }

    } while (counter < node.adjacent_count);

    // Write summary data for each iteration
    write_data("**************************************************");
    // Number of messages passed throughout the network
    int number_of_msg = node.adjacent_count * 2 + counter;
    sprintf(buff, "[Iteration %d] Number of messages passed throughout the network: %d", iteration, number_of_msg);
    write_data(buff);

    // Number of events occurred throughout the network
    sprintf(buff, "[Iteration %d] Number of events occurred throughout the network: %d", iteration, node.adjacent_count);
    write_data(buff);
    write_data("**************************************************\n");
}

// ===================================================================
// SENSOR-NODE TRIGGER FUNCTION
// ===================================================================
void sensor_node(MPI_Comm master_comm, MPI_Comm comm, int cur_procs)
{
    int master_rank;
    MPI_Status status;

    MPI_Comm_rank(master_comm, &master_rank);

    if (master_rank == cur_procs)
    {
        // Get sensor node for rank
        Node node = get_sensor_node(cur_procs);

        int signal = 1;
        int recv_data;

        // Step 1: Trigger to send signal to all adjacent nodes
        for (int i = 0; i < node.adjacent_count; i++)
        {
            int adjacent = node.adjacents[i];

            // Open file to append result
            char signal_msg[MAX_LENGTH];
            sprintf(signal_msg, "[SEND] [Node %d] Signal to adjacent node %d", master_rank, adjacent);
            write_data(signal_msg);

            // Signal adjacent node to process
            MPI_Send(&signal, 1, MPI_INT, adjacent, 0, MPI_COMM_WORLD);
        }

        // Keep received random-number from adjacents
        Adjacent received_result[node.adjacent_count];
        
        // Step 2: Waiting random number from all adjacents
        int index = 0;
        int recv_msg_counter = 0;
        do
        {
            MPI_Recv(&recv_data, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            int sender = status.MPI_SOURCE;
            recv_msg_counter += 1;

            // Add recv_data into received_result
            int existed = 0;
            for (int k = 0; k < index; k++)
            {
                if (received_result[k].random_number == recv_data)
                {
                    existed = 1;
                    received_result[k].adjacents[received_result[k].length] = sender;
                    received_result[k].length += 1;
                }
            }

            if (existed == 0)
            {
                Adjacent info;
                info.master_node = master_rank;
                info.adjacents[0] = sender;
                info.random_number = recv_data;
                info.length = 1;
                received_result[index] = info;

                index += 1;
            }

            // Open file to append result
            char recv_message[MAX_LENGTH];
            sprintf(recv_message, "[RECV] [Node %d] Received data %d from adjacent node %d", master_rank, recv_data, sender);
            write_data(recv_message);

        } while (recv_msg_counter < node.adjacent_count);

        // Step 3: Encrypt message before sending to base-station
        for (int i = 0; i < index; i++)
        {
            Adjacent info1 = received_result[i];
            char *arr_str = array_to_string(info1.adjacents, info1.length);

            // Build message to encrypt before sending to base-station
            char send_msg[100];
            sprintf(send_msg, "Node %d received data %d from adjacents [%s]", info1.master_node, info1.random_number, arr_str);

            // Encrypt message before sending to base-station
            double start_time = MPI_Wtime();
            openmp_encrypt_message(send_msg);
            double end_time = MPI_Wtime();

            // The time taken to Encrypt the message
            char buff[MAX_LENGTH];
            sprintf(buff, "[SEND] [Encrypt] The time taken to Encrypt message for data %d: %lf", 
                    info1.random_number, end_time - start_time);
            write_data(buff);

            // Write sending message to file
            sprintf(buff, "[SEND] [Node %d] Sending data %d of adjacents [%s] to Base-Station\n",
                    info1.master_node, info1.random_number, arr_str);
            write_data(buff);

            // Send encryption message to base-station
            MPI_Send(&send_msg, strlen(send_msg), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }

        // Step 4: Signal base-station to know finish the current iterator
        if (index < recv_msg_counter)
        {
            char str_signal[] = "finish_iterator";
            MPI_Send(str_signal, strlen(str_signal), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }
    else
    {
        Node node = get_sensor_node(cur_procs);
        adjacent_node(master_comm, comm, node);
    }
}

// ===================================================================
// ADJACENT-NODE TRIGGER FUNCTION
// ===================================================================
void adjacent_node(MPI_Comm master_comm, MPI_Comm comm, Node sensor_node)
{
    int rank;
    MPI_Status status;

    MPI_Comm_rank(master_comm, &rank);

    if (is_adjacent(rank, sensor_node.adjacents, sensor_node.adjacent_count))
    {
        int recv_data;
        MPI_Recv(&recv_data, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

        int sender = status.MPI_SOURCE;
        char recv_msg[MAX_LENGTH];
        sprintf(recv_msg, "[RECV] [Adjacent %d] Received signal from node %d", rank, sender);
        write_data(recv_msg);

        // generate random number and send back to sensor-node
        int send_data = get_random_number(1, 10000, rank);      // different random number
        // int send_data = get_random_number(1, 10000, 0);   // have same number

        // write text data to file
        char sending_msg[MAX_LENGTH];
        sprintf(sending_msg, "[SEND] [Adjacent %d] Sending data %d back to node %d\n", rank, send_data, sender);
        write_data(sending_msg);

        // Sending random number back to sensor node
        MPI_Send(&send_data, 1, MPI_INT, sender, 0, MPI_COMM_WORLD);
    }
}

// ===================================================================
// COMMON FUNCTIONS
// ===================================================================
int get_random_process(int pre_proc)
{
    int next_process = get_random_number(1, ROW * COL, 0);
    if (next_process == pre_proc && next_process < (ROW * COL))
    {
        next_process += 1;
    }
    return next_process;
}

// get a random number in range [lower, upper]
int get_random_number(int lower, int upper, int process)
{
    /* intializes random number generator */
    time_t t;
    srand((unsigned)time(&t));

    if (process != 0)
    {
        return (int)(((rand() % upper) + lower) / process);
    }
    else
    {
        return (rand() % upper) + lower + 1;
    }
}

// find the sensor node in WSN from rank, and also get all its adjacent nodes
Node get_sensor_node(int rank)
{
    Node node;
    node.row = (rank - 1) / COL;
    node.col = (rank - 1) % COL;
    node.node_number = rank;
    node.adjacents = get_adjacent_nodes(node.row, node.col);
    node.adjacent_count = node.adjacents[4];
    return node;
}

// get all adjacent nodes of a sensor node
int *get_adjacent_nodes(int row, int col)
{
    int index = 0;
    static int adjacents[5] = {-1, -1, -1, -1, -1};
    if (row > 0)
    {
        adjacents[index] = (row - 1) * COL + col + 1; // up adjacent
        index += 1;
    }
    if (col > 0)
    {
        adjacents[index] = (row * COL) + col; // left adjacent
        index += 1;
    }
    if (col < COL - 1)
    {
        adjacents[index] = (row * COL) + (col + 2); // right adjacent
        index += 1;
    }
    if (row < ROW - 1)
    {
        adjacents[index] = (row + 1) * COL + col + 1; // bottom adjacent
        index += 1;
    }

    // add adjacent count
    adjacents[4] = index;
    return adjacents;
}

// for logging: convert an array integer to string
char *array_to_string(int *arr, int len)
{
    int n = 0;
    static char buffer[50];
    int i;
    for (i = 0; i < len; i++)
    {
        n += sprintf(&buffer[n], "%d", arr[i]);
        if (i < len - 1)
        {
            n += sprintf(&buffer[n], ", ");
        }
    }

    buffer[strlen(buffer)] = '\0';
    return buffer;
}

// for logging: convert a node to string
char *node_to_string(Node node)
{
    char *str_adjacent = array_to_string(node.adjacents, node.adjacent_count);

    static char buffer[255];
    sprintf(buffer, "Node [node %d, row: %d, col: %d], adjacents [%s]",
            node.node_number, node.row, node.col, str_adjacent);

    buffer[strlen(buffer)] = '\0';
    return buffer;
}

// Get current local time as string with format: "mm-dd-yyyy HH:MM:SS"
char *getLocalTime()
{
    static char buffer[26];
    time_t timer;
    struct tm *tm_info;

    time(&timer);
    tm_info = localtime(&timer);

    int hours = tm_info->tm_hour;
    int minutes = tm_info->tm_min;
    int seconds = tm_info->tm_sec;
    int day = tm_info->tm_mday;
    int month = tm_info->tm_mon + 1;
    int year = tm_info->tm_year + 1900;

    sprintf(buffer, "%02d-%02d-%d %02d:%02d:%02d", month, day, year, hours, minutes, seconds);
    buffer[strlen(buffer)] = '\0';

    return buffer;
}

// check if a node existing in adjacent nodes
int is_adjacent(int process, int *adjacents, int len)
{
    int i;
    int existed = 0;
    for (i = 0; i < len; i++)
    {
        if (adjacents[i] == process)
        {
            existed = 1;
            break;
        }
    }
    return existed;
}

// Write message to output file
void write_data(char *message)
{
    char *strTime = getLocalTime();
    // printf("[%s] %s\n", strTime, message);

    FILE *fp = fopen(OUT_FILE_NAME, "a");
    fprintf(fp, "[%s] %s\n", strTime, message);
    fclose(fp);
}

// 
int start_with(char* prefix, char* str)
{
    size_t lenpre = strlen(prefix);
    size_t lenstr = strlen(str);
    if (lenpre > lenstr)
    {
        return 0;
    }
    else if (memcmp(prefix, str, lenpre) == 0){
        return 1;
    }
    return 0;
}

// ================================================================
// Encrypt message using Caesar Cypher Algorithm and OpenMP
// ================================================================
void encrypt_message(char *send_msg)
{
    // The key for encryption is ENCRYPT_KEY that is added to ASCII value
    int i;
    for (i = 0; i < strlen(send_msg); i++)
    {
        send_msg[i] = send_msg[i] + ENCRYPT_KEY;
    }
    // printf("Encrypted Msg: %s\n", send_msg);
}

// Encrypt message using OpenMP
void openmp_encrypt_message(char *send_msg)
{
    int length = strlen(send_msg);
    int start, end;
    int i, tid;

    // Set number of threads
    int nthreads = THREADS;
    omp_set_num_threads(THREADS);
    int chunk = length / THREADS;

    // The key for encryption is ENCRYPT_KEY that is added to ASCII value    
#pragma omp parallel shared(send_msg, nthreads, chunk) private(i, tid)
    {
        tid = omp_get_thread_num();

#pragma omp parallel
        {
            start = tid * chunk;
            end = start + chunk;
            if (tid == 1)
            {
                end += length % THREADS;
            }

#pragma omp for schedule(dynamic, chunk)
            for (i = start; i < end; i++)
            {
                send_msg[i] = send_msg[i] + ENCRYPT_KEY;
            }
        }
    }

    // printf("\nEncrypted Msg: %d: %s\n", length, send_msg);
}

// ================================================================
// Decrypt message using Caesar Cypher Algorithm and OpenMP
// ================================================================
void decrypt_message(char *recv_msg)
{
    // The key for decryption is ENCRYPT_KEY that is subtracted to ASCII value
    int i;
    for (i = 0; i < strlen(recv_msg); i++)
    {
        recv_msg[i] = recv_msg[i] - ENCRYPT_KEY;
    }
    // printf("Decrypted Msg: %s\n", recv_msg);
}

// Decrypt message using OpenMP
void openmp_decrypt_message(char *recv_msg)
{
    int start, end;
    int i, tid;
    int length = strlen(recv_msg);

    // Set number of threads
    int nthreads = THREADS;
    omp_set_num_threads(THREADS);
    int chunk = length / THREADS;

    // The key for decryption is ENCRYPT_KEY that is subtracted to ASCII value
#pragma omp parallel shared(recv_msg, nthreads, chunk) private(i, tid)
    {
        tid = omp_get_thread_num();

#pragma omp parallel
        {
            start = tid * chunk;
            end = start + chunk;
            if (tid == 1)
            {
                end += length % THREADS;
            }

#pragma omp for schedule(dynamic, chunk)
            for (i = start; i < end; i++)
            {
                recv_msg[i] = recv_msg[i] - ENCRYPT_KEY;
            }
        }
    }
    // printf("\nDecrypted Msg: %s\n", recv_msg);
}
