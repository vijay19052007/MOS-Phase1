
#include "mos.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void clear_job_memory(MOSState *s) {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        memset(s->M[i], ' ', WORD_SIZE);
        s->M[i][WORD_SIZE] = '\0';
    }

    memset(s->IR, ' ', WORD_SIZE);
    s->IR[WORD_SIZE] = '\0';

    memset(s->R, ' ', WORD_SIZE);
    s->R[WORD_SIZE] = '\0';

    s->IC = 0;
    s->C = 0;
    s->data_count = 0;
    s->data_index = 0;
    s->job_error = 0;
}

void INIT(MOSState *s) {
    clear_job_memory(s);

    s->jobs_started = 0;
    s->jobs_completed = 0;
    s->jobs_failed = 0;
    s->instruction_count = 0;
    s->read_count = 0;
    s->write_count = 0;
    s->job_id[0] = '\0';
}

static void error_job(MOSState *s, const char *message) {
    if (!s->job_error) {
        fprintf(s->output, "Job %s: ERROR - %s\n",
                s->job_id, message);
    }
    s->job_error = 1;
}

static int get_address(MOSState *s, int *address) {
    if (!isdigit((unsigned char)s->IR[2]) ||
        !isdigit((unsigned char)s->IR[3])) {
        error_job(s, "Invalid instruction address");
        return 0;
    }

    *address = (s->IR[2] - '0') * 10 + (s->IR[3] - '0');

    if (*address < 0 || *address >= MEMORY_SIZE) {
        error_job(s, "Address outside memory");
        return 0;
    }

    return 1;
}

static void read_card(MOSState *s, int address) {
    if (address % 10 != 0 || address + 10 > MEMORY_SIZE) {
        error_job(s, "GD address must be a valid multiple of 10");
        return;
    }

    if (s->data_index >= s->data_count) {
        error_job(s, "No data card available for GD");
        return;
    }

    const char *card = s->data[s->data_index++];

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = i * WORD_SIZE + j;
            s->M[address + i][j] =
                pos < (int)strlen(card) ? card[pos] : ' ';
        }
        s->M[address + i][WORD_SIZE] = '\0';
    }

    s->read_count++;
}

static void write_card(MOSState *s, int address) {
    if (address % 10 != 0 || address + 10 > MEMORY_SIZE) {
        error_job(s, "PD address must be a valid multiple of 10");
        return;
    }

    char line[CARD_SIZE + 1];

    for (int i = 0; i < 10; i++) {
        memcpy(line + i * WORD_SIZE, s->M[address + i], WORD_SIZE);
    }

    line[CARD_SIZE] = '\0';

    int length = CARD_SIZE;
    while (length > 0 && line[length - 1] == ' ') {
        length--;
    }
    line[length] = '\0';

    fprintf(s->output, "%s\n", line);
    s->write_count++;
}

static void execute_job(MOSState *s) {
    int steps = 0;

    while (!s->job_error) {
        if (++steps > 10000) {
            error_job(s, "Instruction limit exceeded");
            break;
        }

        if (s->IC < 0 || s->IC >= MEMORY_SIZE) {
            error_job(s, "Instruction counter outside memory");
            break;
        }

        while (s->IC < MEMORY_SIZE) {
            int blank = 1;

            for (int j = 0; j < WORD_SIZE; j++) {
                if (s->M[s->IC][j] != ' ') {
                    blank = 0;
                    break;
                }
            }

            if (!blank) break;
            s->IC++;
        }

        if (s->IC >= MEMORY_SIZE) {
            error_job(s, "No instruction found in memory");
            break;
        }

        memcpy(s->IR, s->M[s->IC], WORD_SIZE);
        s->IR[WORD_SIZE] = '\0';

        int current_address = s->IC;
        s->IC++;
        s->instruction_count++;

        if (s->IR[0] == 'H') {
            s->jobs_completed++;
            return;
        }

        int address;

        if (!get_address(s, &address)) break;

        if (s->IR[0] == 'G' && s->IR[1] == 'D') {
            read_card(s, address);
        } else if (s->IR[0] == 'P' && s->IR[1] == 'D') {
            write_card(s, address);
        } else if (s->IR[0] == 'L' && s->IR[1] == 'R') {
            memcpy(s->R, s->M[address], WORD_SIZE);
            s->R[WORD_SIZE] = '\0';
        } else if (s->IR[0] == 'S' && s->IR[1] == 'R') {
            memcpy(s->M[address], s->R, WORD_SIZE);
            s->M[address][WORD_SIZE] = '\0';
        } else if (s->IR[0] == 'C' && s->IR[1] == 'R') {
            s->C = (memcmp(s->R, s->M[address], WORD_SIZE) == 0);
        } else if (s->IR[0] == 'B' && s->IR[1] == 'T') {
            if (s->C) s->IC = address;
        } else {
            fprintf(s->output,
                    "Job %s: ERROR - Unknown instruction %.4s at address %02d\n",
                    s->job_id, s->IR, current_address);
            s->job_error = 1;
            break;
        }

        if (s->job_error) break;
    }
}

static void store_program_card(MOSState *s, const char *line,
                               int *program_word) {
    int length = (int)strlen(line);
    if (length > CARD_SIZE) length = CARD_SIZE;

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = i * WORD_SIZE + j;
            s->M[*program_word + i][j] =
                pos < length ? line[pos] : ' ';
        }
        s->M[*program_word + i][WORD_SIZE] = '\0';
    }

    *program_word += 10;
}

static void strip_newline(char *line) {
    size_t length = strlen(line);

    while (length > 0 &&
           (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[--length] = '\0';
    }
}

void LOAD(MOSState *s) {
    char line[512];
    int in_job = 0;
    int in_data = 0;
    int program_word = 0;

    while (fgets(line, sizeof(line), s->input)) {
        strip_newline(line);

        if (strncmp(line, "$AMJ", 4) == 0) {
            if (in_job) {
                error_job(s, "New job started before previous $END");
                s->jobs_failed++;
            }

            clear_job_memory(s);
            in_job = 1;
            in_data = 0;
            program_word = 0;

            size_t id_length = strlen(line + 4);
            if (id_length > 4) id_length = 4;
            memcpy(s->job_id, line + 4, id_length);
            s->job_id[id_length] = '\0';

            s->jobs_started++;
            continue;
        }

        if (!in_job) continue;

        if (strcmp(line, "$DTA") == 0) {
            in_data = 1;
            continue;
        }

        if (strncmp(line, "$END", 4) == 0) {
            execute_job(s);

            if (s->job_error) {
                s->jobs_failed++;
            }

            in_job = 0;
            in_data = 0;
            continue;
        }

        if (in_data) {
            if (s->data_count < MAX_DATA_CARDS) {
                int length = (int)strlen(line);
                if (length > CARD_SIZE) length = CARD_SIZE;

                memcpy(s->data[s->data_count], line, length);
                s->data[s->data_count][length] = '\0';
                s->data_count++;
            } else {
                error_job(s, "Too many data cards");
            }
        } else {
            if (program_word + 10 <= MEMORY_SIZE) {
                store_program_card(s, line, &program_word);
            } else {
                error_job(s, "Program exceeds memory");
            }
        }
    }

    if (in_job) {
        error_job(s, "Missing $END");
        s->jobs_failed++;
    }
}
