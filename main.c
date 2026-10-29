
#include "mos.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *input_file = argc > 1 ? argv[1] : "input.txt";
    const char *output_file = argc > 2 ? argv[2] : "output.txt";

    FILE *in = fopen(input_file, "r");

    if (in == NULL) {
        printf("Error: Cannot open %s\n", input_file);
        return 1;
    }

    FILE *out = fopen(output_file, "w");

    if (out == NULL) {
        printf("Error: Cannot create %s\n", output_file);
        fclose(in);
        return 1;
    }

    MOSState mos;
    memset(&mos, 0, sizeof(mos));

    mos.input = in;
    mos.output = out;

    INIT(&mos);
    LOAD(&mos);

    fprintf(out, "\n========== MOS PHASE 1 SUMMARY ==========\n");
    fprintf(out, "Jobs started   : %d\n", mos.jobs_started);
    fprintf(out, "Jobs completed : %d\n", mos.jobs_completed);
    fprintf(out, "Jobs failed    : %d\n", mos.jobs_failed);
    fprintf(out, "GD instructions: %d\n", mos.read_count);
    fprintf(out, "PD instructions: %d\n", mos.write_count);
    fprintf(out, "Instructions   : %d\n", mos.instruction_count);
    fprintf(out, "=========================================\n");

    fclose(in);
    fclose(out);

    printf("MOS Phase 1 execution completed.\n");
    printf("Check %s for output.\n", output_file);

    return 0;
}
