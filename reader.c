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

#define NUM_GVT_VALS 11
#define NUM_CYCLE_CTRS 11
#define NUM_EV_CTRS 16
#define NUM_MEM 2
#define NUM_KP 16
#define NUM_LP 8
//#define NUM_LP 176

static char doc[] = "Reader for the binary data collection files from ROSS";
static char args_doc[] = "";
static struct argp_option options[] = {
    {"filename",  'f',  "str",  0,  "path of file to read" },
    {"filetype", 't',  "n",  0,  "type of file to read; 0: GVT data, 1: real time data, 2: event data" },
    {"granularity", 'g', "n", 0, "granularity of data collected; 0: PE, 1: KP/LP"},
    { 0 }
};

struct arguments
{
    char *filename;
    int filetype;
    int granularity;
};

typedef double tw_stime;
typedef unsigned long tw_peid;
typedef unsigned long long tw_stat;
typedef long tw_node;
typedef uint64_t tw_lpid;
#ifdef BGQ
typedef unsigned long long tw_clock; // BGQ
#else
typedef uint64_t tw_clock; // x86
#endif

typedef struct gvt_line gvt_line;
struct gvt_line{
    unsigned short id;
    float ts;
    unsigned int values[NUM_GVT_VALS];
    int nsend_net_remote;
    int net_events;
    float efficiency;
} __attribute__((__packed__));

typedef struct gvt_line_lps gvt_line_lps;
struct gvt_line_lps{
    unsigned short id;
    float ts;
    unsigned int values[4];
    int net_events;
    float efficiency;
    unsigned int kp_vals[NUM_KP][2];
    unsigned int lp_vals[NUM_LP][4];
    int nsend_net_remote[NUM_LP];
} __attribute__((__packed__));

typedef struct rt_line rt_line;
struct rt_line{
    unsigned short id;
    float ts;
    float gvt;
    float time_ahead_gvt[NUM_KP];
    tw_clock cycles[NUM_CYCLE_CTRS];
    unsigned int ev_counters[NUM_EV_CTRS];
    //size_t mem[NUM_MEM];
} __attribute__((__packed__));

typedef struct rt_line_lps rt_line_lps;
struct rt_line_lps {
    unsigned short id;
    float ts;
    float gvt;
    float time_ahead_gvt[NUM_KP];
    tw_clock cycles[NUM_CYCLE_CTRS];
    unsigned int ev_counters[5];
    unsigned int kp_counters[NUM_KP][2];
    unsigned int lp_counters[NUM_LP][4];
    int nsend_net_remote[NUM_LP];
    //size_t mem[NUM_MEM];
} __attribute__((__packed__));

typedef struct event_line event_line;
struct event_line{
    tw_lpid src_lp;
    tw_lpid dest_lp;
    tw_stime recv_ts_vt;
    tw_stime recv_ts_rt;
    int event_type;
} __attribute__((__packed__));

typedef enum {
    GVT,
    RT,
    EVENT
} file_types;

static error_t parse_opt (int key, char *arg, struct argp_state *state);
void gvt_read(FILE *file, FILE *output);
void gvt_read_lps(FILE *file, FILE *output);
void rt_read(FILE *file, FILE *output);
void rt_read_lps(FILE *file, FILE *output);
void event_read(FILE *file, FILE *output);
void print_gvt_struct(FILE *output, gvt_line *line);
void print_gvt_lps_struct(FILE *output, gvt_line_lps *line);
void print_rt_struct(FILE *output, rt_line *line);
void print_rt_lps_struct(FILE *output, rt_line_lps *line);
void print_event_struct(FILE *output, event_line *line);

static struct argp argp = { options, parse_opt, args_doc, doc };

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

    if (args.filetype == GVT && !args.granularity)
        gvt_read(file, output);
    if (args.filetype == GVT && args.granularity)
        gvt_read_lps(file, output);
    if (args.filetype == RT && !args.granularity)
        rt_read(file, output);
    if (args.filetype == RT && args.granularity)
        rt_read_lps(file, output);
    if (args.filetype == EVENT)
        event_read(file, output);

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
        case 'g':
            args->granularity = atoi(arg);
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

void gvt_read_lps(FILE *file, FILE *output)
{
    int i;
    gvt_line_lps myline;
    fprintf(output, "PE_ID,GVT,all_reduce_count,events_aborted,event_ties,fossil_collects,net_events,efficiency,");
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "KP-%d_total_rollbacks,KP-%d_secondary_rollbacks,", i, i);
    for (i = 0; i < NUM_LP; i++)
        fprintf(output, "LP-%d_events_processed,LP-%d_events_rolled_back,LP-%d_remote_sends,LP-%d_remote_recvs,", i, i, i, i);
    for (i = 0; i < NUM_LP; i++)
    {
        if (i == NUM_LP - 1)
            fprintf(output, "LP-%d_remote_events\n", i);
        else
            fprintf(output, "LP-%d_remote_events,", i);
    }
    while (!feof(file))
    {
        fread(&myline, sizeof(gvt_line_lps), 1, file);
        print_gvt_lps_struct(output, &myline);
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
            "primary_rb\n");
    fread(&myline, sizeof(rt_line), 1, file);
    while (!feof(file))
    {
        print_rt_struct(output, &myline);
        fread(&myline, sizeof(rt_line), 1, file);
    }

}

void rt_read_lps(FILE *file, FILE *output)
{
    int i;
    rt_line_lps myline;
    fprintf(output, "PE_ID,real_TS,current_GVT,");
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "KP-%d_time_ahead_GVT,", i);
    fprintf(output, "network_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancelq_CC,"
            "avl_CC,buddy_CC,lz4_CC,aborted_events,pq_size,event_ties,fossil_collect_attmepts,num_GVT_comps,");
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "KP-%d_total_rollbacks,KP-%d_secondary_rollbacks,", i, i);
    for (i = 0; i < NUM_LP; i++)
        fprintf(output, "LP-%d_events_processed,LP-%d_events_rolled_back,LP-%d_remote_sends,LP-%d_remote_recvs,", i, i, i, i);
    for (i = 0; i < NUM_LP; i++)
    {
        if (i == NUM_LP - 1)
            fprintf(output, "LP-%d_remote_events\n", i);
        else
            fprintf(output, "LP-%d_remote_events,", i);
    }
    fread(&myline, sizeof(rt_line_lps), 1, file);
    while (!feof(file))
    {
        print_rt_lps_struct(output, &myline);
        fread(&myline, sizeof(rt_line_lps), 1, file);
    }

}

void event_read(FILE *file, FILE *output)
{
    event_line myline;
    fprintf(output, "src_lp,dest_lp,recv_ts_vt,recv_ts_rt,event_type\n");
    while (!feof(file))
    {
        fread(&myline, sizeof(event_line), 1, file);
        print_event_struct(output, &myline);
    }
}

void print_gvt_struct(FILE *output, gvt_line *line)
{
    int i;
    fprintf(output, "%u,%f,", line->id, line->ts);
    for (i = 0; i < NUM_GVT_VALS; i++)
        fprintf(output, "%u,", line->values[i]);
    fprintf(output, "%d,%d,%f\n", line->nsend_net_remote, line->net_events, line->efficiency);
}

void print_gvt_lps_struct(FILE *output, gvt_line_lps *line)
{
    int i,j;
    fprintf(output, "%u,%f,", line->id, line->ts);
    for (i = 0; i < 4; i++)
        fprintf(output, "%u,", line->values[i]);
    fprintf(output, "%d,%f,", line->net_events, line->efficiency);
    for (i = 0; i < NUM_KP; i++)
    {
        for (j = 0; j < 2; j++)
            fprintf(output, "%u,", line->kp_vals[i][j]);
    }
    for (i = 0; i < NUM_LP; i++)
    {
        for (j = 0; j < 4; j++)
            fprintf(output, "%u,", line->lp_vals[i][j]);
    }
    for (i = 0; i < NUM_LP; i++)
    {
        if (i == NUM_LP - 1)
            fprintf(output, "%d\n", line->nsend_net_remote[i]);
        else
            fprintf(output, "%d,", line->nsend_net_remote[i]);
    }
}

void print_rt_struct(FILE *output, rt_line *line)
{
    int i;
    fprintf(output, "%u,%f,%f,", line->id, line->ts, line->gvt);
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "%f,", line->time_ahead_gvt[i]);
    for (i = 0; i < NUM_CYCLE_CTRS; i++)
#ifdef BGQ
        fprintf(output, "%u,", line->cycles[i]);
#else
        fprintf(output, "%"PRIu64",", line->cycles[i]);
#endif
    for (i = 0; i < NUM_EV_CTRS; i++)
    {
        fprintf(output, "%u", line->ev_counters[i]);
        if (i != NUM_EV_CTRS-1)
            fprintf(output, ",");
        else
            fprintf(output, "\n");
    }
    /*
     *for (i = 0; i < NUM_MEM; i++)
     *{
     *    fprintf(output, "%d", line->mem[i]);
     *    if (i != NUM_MEM-1)
     *        fprintf(output, ",");
     *    else
     *        fprintf(output, "\n");
     *}
     */
}

void print_rt_lps_struct(FILE *output, rt_line_lps *line)
{
    int i,j;
//    fprintf(output, "\n");
    fprintf(output, "%u,%f,%f,", line->id, line->ts, line->gvt);
//    fprintf(output, "\n");
    for (i = 0; i < NUM_KP; i++)
        fprintf(output, "%f,", line->time_ahead_gvt[i]);
//    fprintf(output, "\n");
    for (i = 0; i < NUM_CYCLE_CTRS; i++)
#ifdef BGQ
        fprintf(output, "%u,", line->cycles[i]);
#else
        fprintf(output, "%"PRIu64",", line->cycles[i]);
#endif
//    fprintf(output, "\n");
    for (i = 0; i < 5; i++)
        fprintf(output, "%u,", line->ev_counters[i]);
//    fprintf(output, "\n");
    for (i = 0; i < NUM_KP; i++)
    {
        for (j = 0; j < 2; j++)
            fprintf(output, "%u,", line->kp_counters[i][j]);
    }
//    fprintf(output, "\n");
    for (i = 0; i < NUM_LP; i++)
    {
        for (j = 0; j < 4; j++)
            fprintf(output, "%u,", line->lp_counters[i][j]);
//        fprintf(output, "\n");
    }
//    fprintf(output, "\n");
    for (i = 0; i < NUM_LP; i++)
    {
        fprintf(output, "%d", line->nsend_net_remote[i]);
        if (i != NUM_LP-1)
            fprintf(output, ",");
        else
            fprintf(output, "\n");
    }
//    fprintf(output, "\n");
    /*
     *for (i = 0; i < NUM_MEM; i++)
     *{
     *    fprintf(output, "%d", line->mem[i]);
     *    if (i != NUM_MEM-1)
     *        fprintf(output, ",");
     *    else
     *        fprintf(output, "\n");
     *}
     */
}

void print_event_struct(FILE *output, event_line *line)
{
    fprintf(output, "%"PRIu64",%"PRIu64",%f,%f,%d\n", line->src_lp, line->dest_lp, line->recv_ts_vt, line->recv_ts_rt, line->event_type);
}
