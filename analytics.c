#include "analytics.h"
#include <stdio.h>

void analytics_write_report(
    const MOSState *s,
    const char *filename
) {
    FILE *fp = fopen(filename, "w");

    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot create %s\n", filename);
        return;
    }

    int total_cpu = 0;
    int total_io = 0;
    int total_termination = 0;
    int total_load = 0;
    int total_turnaround = 0;
    int total_waiting = 0;
    int jobs_with_times = 0;

    fprintf(fp, "MOS PHASE 1 SIMULATOR - ANALYTICS REPORT\n");
    fprintf(fp, "=========================================\n\n");

    fprintf(fp, "Total jobs started : %d\n", s->jobs_started);
    fprintf(fp, "Jobs completed     : %d\n", s->jobs_completed);
    fprintf(fp, "Jobs failed        : %d\n", s->jobs_failed);
    fprintf(fp, "Total instructions : %d\n", s->instruction_count);
    fprintf(fp, "Total GD operations: %d\n", s->read_count);
    fprintf(fp, "Total PD operations: %d\n", s->write_count);
    fprintf(fp, "Simulation clock   : %d ticks\n\n", s->clock);

    fprintf(fp, "JOB-WISE STATISTICS\n");
    fprintf(fp, "-----------------------------------------------------------------------------------------------\n");
    fprintf(fp,
        "%-6s %-7s %-7s %-7s %-9s %-11s %-6s %-6s %-6s %-7s\n",
        "Job", "Arrival", "Start", "Finish", "Waiting",
        "Turnaround", "CPU", "I/O", "Load", "Status");
    fprintf(fp, "-----------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < s->jobs_started && i < MAX_JOBS; i++) {
        const JobStats *j = &s->jobs[i];

        int waiting = j->start_time - j->arrival_time;
        int turnaround = j->finish_time - j->arrival_time;

        if (waiting < 0) waiting = 0;
        if (turnaround < 0) turnaround = 0;

        fprintf(fp,
            "%-6s %-7d %-7d %-7d %-9d %-11d %-6d %-6d %-6d %-7s\n",
            j->job_id,
            j->arrival_time,
            j->start_time,
            j->finish_time,
            waiting,
            turnaround,
            j->cpu_time,
            j->io_time,
            j->load_time,
            j->failed ? "FAILED" :
                (j->completed ? "COMPLETED" : "INCOMPLETE"));

        total_cpu += j->cpu_time;
        total_io += j->io_time;
        total_termination += j->termination_time;
        total_load += j->load_time;
        total_waiting += waiting;
        total_turnaround += turnaround;
        jobs_with_times++;
    }

    fprintf(fp, "-----------------------------------------------------------------------------------------------\n");

    fprintf(fp, "\nTIME BREAKDOWN\n");
    fprintf(fp, "Card loading time : %d ticks\n", total_load);
    fprintf(fp, "CPU time          : %d ticks\n", total_cpu);
    fprintf(fp, "I/O time           : %d ticks\n", total_io);
    fprintf(fp, "Termination time  : %d ticks\n", total_termination);

    if (jobs_with_times > 0) {
        fprintf(fp, "\nAVERAGES (all recorded jobs)\n");
        fprintf(fp, "Average waiting time    : %.2f ticks\n",
                (double)total_waiting / jobs_with_times);
        fprintf(fp, "Average turnaround time : %.2f ticks\n",
                (double)total_turnaround / jobs_with_times);
    }

    fprintf(fp, "\nTiming assumptions:\n");
    fprintf(fp, "Each program or data card loaded: 1 tick\n");
    fprintf(fp, "Each fetched instruction        : 1 CPU tick\n");
    fprintf(fp, "Each successful GD or PD        : 5 I/O ticks\n");
    fprintf(fp, "Each executed H termination     : 2 ticks\n");

    fclose(fp);
}