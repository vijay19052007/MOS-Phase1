
#ifndef MOS_H
#define MOS_H

#include <stdio.h>

#define MEMORY_SIZE 100
#define WORD_SIZE 4
#define CARD_SIZE 40
#define MAX_DATA_CARDS 100

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

    int job_error;
    char job_id[5];
} MOSState;

void INIT(MOSState *s);
/// @brief 
/// @param s 
void LOAD(MOSState *s);

#endif
