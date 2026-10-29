#ifndef MOS_H
#define MOS_H

#include <stdio.h>

#define MEMORY_SIZE 100
#define WORD_SIZE 4
#define CARD_SIZE 40
#define MAX_DATA_CARDS 100
#define MAX_JOBS 100

typedef struct {
    char job_id[5];

    int arrival_time;
    int start_time;
    int finish_time;

    int load_time;
    int cpu_time;
    int io_time;
    int termination_time;

    int instruction_count;
    int read_count;
    int write_count;

    int completed;
    int failed;
} JobStats;

typedef struct {
    char M[MEMORY_SIZE][WORD_SIZE + 1];
    char IR[WORD_SIZE + 1];
    char R[WORD_SIZE + 1];

    int IC;
    int C;

    char data[MAX_DATA_CARDS][CARD_SIZE + 1];
    int data_count;
    int data_index;

    FILE *input;
    FILE *output;

    int jobs_started;
    int jobs_completed;
    int jobs_failed;
    int instruction_count;
    int read_count;
    int write_count;

    int clock;
    int job_error;
    int job_terminated;

    char job_id[5];

    JobStats jobs[MAX_JOBS];
    int current_job;
} MOSState;

void INIT(MOSState *s);
void LOAD(MOSState *s);

#endif