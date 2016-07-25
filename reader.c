#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#ifdef MACOS
#include "mac/argp.h"
#else
#include <argp.h>
#endif

#define GVT 0
#define RT 1
#define NUM_GVT_VALS 11
#define NUM_CYCLE_CTRS 11
#define NUM_EV_CTRS 16
#define NUM_MEM 2
#define NUM_KP 16

static char doc[] = "Reader for the binary data collection files from ROSS";
static char args_doc[] = "";
static struct argp_option options[] = {
    {"filename",  'f',  "str",  0,  "path of file to read" },
    {"filetype", 't',  "n",  0,  "type of file to read; 0: GVT data, 1: real time data" },
    { 0 }
};

struct arguments
{
    char *filename;
    int filetype;
};

typedef double tw_stime;
typedef unsigned long tw_peid;
typedef unsigned long long tw_stat;
typedef long tw_node;
#ifdef BGQ
typedef unsigned long long tw_clock; // BGQ
#else
typedef uint64_t tw_clock; // x86
#endif

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
    tw_stime gvt;
    tw_stime time_ahead_gvt[NUM_KP];
    tw_clock cycles[NUM_CYCLE_CTRS];
    tw_stat ev_counters[NUM_EV_CTRS];
    size_t mem[NUM_MEM];
} rt_line;

static error_t parse_opt (int key, char *arg, struct argp_state *state);
void gvt_read(FILE *file, FILE *output);
void rt_read(FILE *file, FILE *output);
void print_gvt_struct(FILE *output, gvt_line *line);
void print_rt_struct(FILE *output, rt_line *line);

static struct argp argp = { options, parse_opt, args_doc, doc };
/*
 * argv[1]: filename to read from
 * argv[2]: which type of file are you reading
 */
int main(int argc, char **argv) 
{
    FILE *file, *output;
    char filename[2048];
    struct arguments args;

    /* Default values. */
    args.filename = "-";
    args.filetype = 0;

    /* Parse our arguments; every option seen by parse_opt will
     *      be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &args);

    file = fopen(args.filename, "r");
    sprintf(filename, "%s.csv", args.filename);
    output = fopen(filename, "w");

    if (args.filetype == GVT)
        gvt_read(file, output);
    if (args.filetype == RT)
        rt_read(file, output);

    fclose(file);
    fclose(output);

    return EXIT_SUCCESS;
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
    *      know is a pointer to our arguments structure. */
    struct arguments *args = state->input;

    switch (key)
    {
        case 't':
            args->filetype = atoi(arg);
            break;
        case 'f':
            args->filename = arg;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


void gvt_read(FILE *file, FILE *output)
{
    gvt_line myline;
    fprintf(output, "PE_ID,GVT,all_reduce_count,events_processed,events_aborted,events_rolled_back,event_ties,total_rollbacks,primary_rollbacks,"
            "secondary_rollbacks,fossil_collects,network_sends,network_recvs,remote_events,net_events,efficiency\n");
    while (!feof(file))
    {
        fread(&myline, sizeof(gvt_line), 1, file);
        print_gvt_struct(output, &myline);
    }
}

void rt_read(FILE *file, FILE *output)
{
    int i;
    rt_line myline;
    fprintf(output, "PE_ID,real_TS,current_GVT,");
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "KP-%d_time_ahead_GVT,", i);
    fprintf(output, "network_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancelq_CC,"
            "avl_CC,buddy_CC,lz4_CC,aborted_events,pq_size,network_remote_sends,local_remote_sends,network_sends,network_recvs,"
            "remote_rb,event_ties,fossil_collect_attempts,num_GVTs,events_processed,events_rolled_back,total_rollbacks,secondary_rollbacks,net_events,"
            "primary_rb,mem_allocated,mem_wasted\n");
    fread(&myline, sizeof(rt_line), 1, file);
    while (!feof(file))
    {
        print_rt_struct(output, &myline);
        fread(&myline, sizeof(rt_line), 1, file);
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
    fprintf(output, "%lu,%f,%f,", line->id, line->ts, line->gvt);
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "%f,", line->time_ahead_gvt[i]);
    for (i = 0; i < NUM_CYCLE_CTRS; i++)
#ifdef BGQ
        fprintf(output, "%llu,", line->cycles[i]);
#else
        fprintf(output, "%"PRIu64",", line->cycles[i]);
#endif
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
