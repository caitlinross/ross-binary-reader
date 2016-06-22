#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define GVT 0
#define RT 1
#define NUM_GVT_VALS 11
#define NUM_CYCLE_CTRS 11
#define NUM_EV_CTRS 18
#define NUM_MEM 2
#define NUM_KP 4

typedef double tw_stime;
typedef unsigned long tw_peid;
typedef unsigned long long tw_stat;
typedef long tw_node;
typedef uint64_t tw_clock; // x86
//typedef unsigned long long tw_clock // BGQ

typedef struct {
    tw_node id;
    tw_stime ts;
    tw_stat values[NUM_GVT_VALS];
    long long nsend_net_remote;
    long long net_events;
    double efficiency;
} gvt_line;

typedef struct {
    tw_peid id;
    tw_stime ts;
    tw_stime time_ahead_gvt[NUM_KP];
    tw_clock cycles[NUM_CYCLE_CTRS];
    tw_stat ev_counters[NUM_EV_CTRS];
    size_t mem[NUM_MEM];
} rt_line;

void gvt_read(FILE *file, FILE *output);
void rt_read(FILE *file, FILE *output);
void print_gvt_struct(FILE *output, gvt_line *line);
void print_rt_struct(FILE *output, rt_line *line);

int main(int argc, char **argv) 
{
    FILE *file, *output;
    char filename[1024];
    file = fopen(argv[1], "r");
    sprintf(filename, "%s.csv", argv[1]);
    output = fopen(filename, "w");

    int num_kps = atoi(argv[2]);
    int file_type = atoi(argv[3]); /* 0: GVT, 1: rt sampling  */
    if (file_type == GVT)
        gvt_read(file, output);
    if (file_type == RT)
        rt_read(file, output);

    fclose(file);
    fclose(output);
}

void gvt_read(FILE *file, FILE *output)
{
    gvt_line myline;
    int rc, index = 0;

    while (!feof(file))
    {
        rc = fread(&myline, sizeof(gvt_line), 1, file);
        print_gvt_struct(output, &myline);
    }
}

void rt_read(FILE *file, FILE *output)
{
    rt_line myline;
    int rc, index = 0;

    while (!feof(file))
    {
        rc = fread(&myline, sizeof(rt_line), 1, file);
        print_rt_struct(output, &myline);
    }

}

void print_gvt_struct(FILE *output, gvt_line *line)
{
    int i;
    fprintf(output, "%ld,%f,", line->id, line->ts);
    for (i = 0; i < NUM_GVT_VALS; i++)
        fprintf(output, "%llu,", line->values[i]);
    fprintf(output, "%lld,%lld,%f\n", line->nsend_net_remote, line->net_events, line->efficiency);
}

void print_rt_struct(FILE *output, rt_line *line)
{
    int i;
    fprintf(output, "%lu,%f,", line->id, line->ts);
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "%f,", line->time_ahead_gvt[i]);
    for (i = 0; i < NUM_CYCLE_CTRS; i++)
        fprintf(output, "%ld,", line->cycles[i]);
    for (i = 0; i < NUM_EV_CTRS; i++)
        fprintf(output, "%llu,", line->ev_counters[i]);
    for (i = 0; i < NUM_MEM; i++)
    {
        fprintf(output, "%ld", line->mem[i]);
        if (i != NUM_MEM-1)
            fprintf(output, ",");
        else
            fprintf(output, "\n");
    }
}
