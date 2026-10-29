
#include "mos.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static JobStats *current_stats(MOSState *s) {
    if (s->current_job < 0 ||
        s->current_job >= s->jobs_started ||
        s->current_job >= MAX_JOBS) {
        return NULL;
    }

    return &s->jobs[s->current_job];
}

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
    s->job_terminated = 0;
}

void INIT(MOSState *s) {
    FILE *input = s->input;
    FILE *output = s->output;

    memset(s, 0, sizeof(*s));

    s->input = input;
    s->output = output;
    s->current_job = -1;

    clear_job_memory(s);
}

static void error_job(MOSState *s, const char *message) {
    if (!s->job_error) {
        fprintf(s->output, "Job %s: ERROR - %s\n",
                s->job_id, message);
    }

    s->job_error = 1;
}

static void advance_clock(MOSState *s, int ticks) {
    if (ticks > 0) {
        s->clock += ticks;
    }
}

static void record_card_load(MOSState *s) {
    JobStats *j = current_stats(s);

    advance_clock(s, 1);

    if (j != NULL) {
        j->load_time++;
    }
}

static int is_blank_word(const char *word) {
    for (int i = 0; i < WORD_SIZE; i++) {
        if (word[i] != ' ') {
            return 0;
        }
    }

    return 1;
}

static int valid_opcode(const char *word) {
    if (word[0] == 'H' &&
        word[1] == ' ' &&
        word[2] == ' ' &&
        word[3] == ' ') {
        return 1;
    }

    return (word[0] == 'L' && word[1] == 'R') ||
           (word[0] == 'S' && word[1] == 'R') ||
           (word[0] == 'C' && word[1] == 'R') ||
           (word[0] == 'B' && word[1] == 'T') ||
           (word[0] == 'G' && word[1] == 'D') ||
           (word[0] == 'P' && word[1] == 'D');
}

static int get_address(MOSState *s, int *address) {
    if (!isdigit((unsigned char)s->IR[2]) ||
        !isdigit((unsigned char)s->IR[3])) {
        error_job(s, "Invalid instruction address");
        return 0;
    }

    *address = (s->IR[2] - '0') * 10 +
               (s->IR[3] - '0');

    if (*address < 0 || *address >= MEMORY_SIZE) {
        error_job(s, "Address outside virtual memory");
        return 0;
    }

    return 1;
}

static int read_card(
    MOSState *s,
    int address,
    int instruction_present[MEMORY_SIZE]
) {
    if (address % 10 != 0 ||
        address + 10 > MEMORY_SIZE) {
        error_job(s, "GD address must be a valid block start");
        return 0;
    }

    if (s->data_index >= s->data_count) {
        error_job(s, "No data card available for GD");
        return 0;
    }

    const char *card = s->data[s->data_index++];
    size_t card_length = strlen(card);

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = i * WORD_SIZE + j;

            s->M[address + i][j] =
                pos < (int)card_length ? card[pos] : ' ';
        }

        s->M[address + i][WORD_SIZE] = '\0';
        instruction_present[address + i] = 0;
    }

    s->read_count++;

    JobStats *j = current_stats(s);

    if (j != NULL) {
        j->read_count++;
        j->io_time += 5;
    }

    advance_clock(s, 5);

    return 1;
}

static int write_card(MOSState *s, int address) {
    if (address % 10 != 0 ||
        address + 10 > MEMORY_SIZE) {
        error_job(s, "PD address must be a valid block start");
        return 0;
    }

    char line[CARD_SIZE + 1];

    for (int i = 0; i < 10; i++) {
        memcpy(line + i * WORD_SIZE,
               s->M[address + i], WORD_SIZE);
    }

    line[CARD_SIZE] = '\0';

    int length = CARD_SIZE;

    while (length > 0 && line[length - 1] == ' ') {
        length--;
    }

    line[length] = '\0';

    fprintf(s->output, "%s\n", line);

    s->write_count++;

    JobStats *j = current_stats(s);

    if (j != NULL) {
        j->write_count++;
        j->io_time += 5;
    }

    advance_clock(s, 5);

    return 1;
}

static int store_program_card(
    MOSState *s,
    const char *line,
    int *program_word,
    int instruction_present[MEMORY_SIZE]
) {
    int length = (int)strlen(line);

    if (length > CARD_SIZE) {
        error_job(s, "Program card exceeds 40 characters");
        return 0;
    }

    if (*program_word + 10 > MEMORY_SIZE) {
        error_job(s, "Program exceeds virtual memory");
        return 0;
    }

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = i * WORD_SIZE + j;

            s->M[*program_word + i][j] =
                pos < length ? line[pos] : ' ';
        }

        s->M[*program_word + i][WORD_SIZE] = '\0';

        instruction_present[*program_word + i] =
            !is_blank_word(s->M[*program_word + i]);
    }

    *program_word += 10;
    record_card_load(s);

    return 1;
}

static void execute_job(
    MOSState *s,
    int instruction_present[MEMORY_SIZE]
) {
    int steps = 0;

    while (!s->job_error && !s->job_terminated) {
        if (++steps > 10000) {
            error_job(s, "Instruction limit exceeded");
            break;
        }

        if (s->IC < 0 || s->IC >= MEMORY_SIZE) {
            error_job(s, "Instruction counter outside memory");
            break;
        }

        while (s->IC < MEMORY_SIZE &&
               !instruction_present[s->IC]) {
            s->IC++;
        }

        if (s->IC >= MEMORY_SIZE) {
            error_job(s, "No instruction found in program memory");
            break;
        }

        int current_address = s->IC;

        memcpy(s->IR, s->M[s->IC], WORD_SIZE);
        s->IR[WORD_SIZE] = '\0';
        s->IC++;

        if (!valid_opcode(s->IR)) {
            error_job(s, "Invalid instruction in program memory");
            break;
        }

        s->instruction_count++;

        JobStats *j = current_stats(s);

        if (j != NULL) {
            j->instruction_count++;
            j->cpu_time++;
        }

        advance_clock(s, 1);

        if (s->IR[0] == 'H') {
            advance_clock(s, 2);

            if (j != NULL) {
                j->termination_time += 2;
                j->completed = 1;
            }

            s->job_terminated = 1;
            s->jobs_completed++;
            return;
        }

        int address;

        if (!get_address(s, &address)) {
            break;
        }

        if (s->IR[0] == 'G' && s->IR[1] == 'D') {
            read_card(s, address, instruction_present);
        } else if (s->IR[0] == 'P' && s->IR[1] == 'D') {
            write_card(s, address);
        } else if (s->IR[0] == 'L' && s->IR[1] == 'R') {
            memcpy(s->R, s->M[address], WORD_SIZE);
            s->R[WORD_SIZE] = '\0';
        } else if (s->IR[0] == 'S' && s->IR[1] == 'R') {
            memcpy(s->M[address], s->R, WORD_SIZE);
            s->M[address][WORD_SIZE] = '\0';
            instruction_present[address] = 0;
        } else if (s->IR[0] == 'C' && s->IR[1] == 'R') {
            s->C = (memcmp(s->R, s->M[address],
                           WORD_SIZE) == 0);
        } else if (s->IR[0] == 'B' && s->IR[1] == 'T') {
            if (s->C) {
                if (!instruction_present[address]) {
                    error_job(s, "Invalid branch target");
                    break;
                }

                if (!valid_opcode(s->M[address])) {
                    error_job(s, "Branch target is not a valid instruction");
                    break;
                }

                s->IC = address;
            }
        } else {
            fprintf(s->output,
                    "Job %s: ERROR - Unknown instruction %.4s "
                    "at address %02d\n",
                    s->job_id, s->IR, current_address);

            s->job_error = 1;
            break;
        }
    }
}

static void strip_newline(char *line) {
    size_t length = strlen(line);

    while (length > 0 &&
           (line[length - 1] == '\n' ||
            line[length - 1] == '\r')) {
        line[--length] = '\0';
    }
}

static void finish_failed_job(MOSState *s) {
    JobStats *j = current_stats(s);

    if (j != NULL && !j->failed && !j->completed) {
        j->failed = 1;
        s->jobs_failed++;
    }
}

void LOAD(MOSState *s) {
    char line[512];

    int in_job = 0;
    int in_data = 0;
    int program_word = 0;

    int instruction_present[MEMORY_SIZE] = {0};

    while (fgets(line, sizeof(line), s->input) != NULL) {
        strip_newline(line);

        if (strncmp(line, "$AMJ", 4) == 0) {
            if (in_job) {
                error_job(s, "New job started before previous $END");
                finish_failed_job(s);

                JobStats *previous = current_stats(s);

                if (previous != NULL) {
                    previous->finish_time = s->clock;
                }
            }

            if (s->jobs_started >= MAX_JOBS) {
                fprintf(s->output,
                        "ERROR: Maximum job limit (%d) exceeded\n",
                        MAX_JOBS);

                in_job = 0;
                in_data = 0;
                continue;
            }

            clear_job_memory(s);
            memset(instruction_present, 0,
                   sizeof(instruction_present));
            memset(s->job_id, 0, sizeof(s->job_id));

            in_job = 1;
            in_data = 0;
            program_word = 0;

            s->current_job = s->jobs_started;
            s->jobs_started++;

            JobStats *j = current_stats(s);

            if (j != NULL) {
                memset(j, 0, sizeof(*j));
            }

            size_t id_length = strlen(line + 4);

            if (id_length > 4) {
                id_length = 4;
            }

            memcpy(s->job_id, line + 4, id_length);
            s->job_id[id_length] = '\0';

            if (j != NULL) {
                memcpy(j->job_id, s->job_id, sizeof(j->job_id));
                j->arrival_time = s->clock;
                j->start_time = s->clock;
            }

            continue;
        }

        if (!in_job) {
            continue;
        }

        if (strcmp(line, "$DTA") == 0) {
            in_data = 1;
            continue;
        }

        if (strncmp(line, "$END", 4) == 0) {
            JobStats *j = current_stats(s);

            if (j != NULL) {
                j->start_time = s->clock;
            }

            if (!s->job_error) {
                execute_job(s, instruction_present);
            }

            if (!s->job_terminated && !s->job_error) {
                error_job(s, "Program ended without H");
            }

            if (s->job_error) {
                finish_failed_job(s);
            }

            if (j != NULL) {
                j->finish_time = s->clock;
            }

            in_job = 0;
            in_data = 0;
            continue;
        }

        if (in_data) {
            if (s->data_count >= MAX_DATA_CARDS) {
                error_job(s, "Too many data cards");
                continue;
            }

            int length = (int)strlen(line);

            if (length > CARD_SIZE) {
                error_job(s, "Data card exceeds 40 characters");
                continue;
            }

            memcpy(s->data[s->data_count], line, (size_t)length);
            s->data[s->data_count][length] = '\0';
            s->data_count++;

            record_card_load(s);
        } else {
            store_program_card(s, line, &program_word,
                               instruction_present);
        }
    }

    if (in_job) {
        error_job(s, "Missing $END");
        finish_failed_job(s);

        JobStats *j = current_stats(s);

        if (j != NULL) {
            j->finish_time = s->clock;
        }
    }
}
