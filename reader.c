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

int NUM_PE = 0;
int *lps_per_pe;

static char doc[] = "Reader for the binary data collection files from ROSS";
static char args_doc[] = "";
static struct argp_option options[] = {
    {"filename",  'f',  "str",  0,  "path of file to read" },
    {"filetype", 't',  "n",  0,  "type of file to read; 0: GVT data, 1: real time data, 2: event data" },
    {"granularity", 'g', "n", 0, "granularity of data collected; 0: PE, 1: KP/LP"},
    {"num-pe", 'p', "n", 0, "number of PEs"},
    { 0 }
};

struct arguments
{
    char *filename;
    int filetype;
    int granularity;
    int num_pes;
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
    unsigned int **lp_vals;
    int *nsend_net_remote;
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
    unsigned int **lp_counters;
    int *nsend_net_remote;
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
void gvt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out);
void rt_read(FILE *file, FILE *output);
void rt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out);
void event_read(FILE *file, FILE *output);
void print_gvt_struct(FILE *output, gvt_line *line);
void print_gvt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, gvt_line_lps *line, int num_lps);
void print_rt_struct(FILE *output, rt_line *line);
void print_rt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, rt_line_lps *line, int num_lps);
void print_event_struct(FILE *output, event_line *line);
char *get_prefix(char *filename);
void read_lps_per_pe(FILE *file, int *lps_per_pe);

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv)
{
    FILE *file, *output, *rfile, *lp_out, *kp_out;
    char filename[512];
    char tmpname[512];
    struct arguments args = {0};
    char readme_file[512];

    /* Default values. */
    args.filename = "-";
    args.filetype = 0;

    /* Parse our arguments; every option seen by parse_opt will
     *      be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &args);

    file = fopen(args.filename, "r");
    char *prefix = get_prefix(args.filename);
    sprintf(readme_file, "%sREADME.txt", prefix);
    rfile = fopen(readme_file, "r");
    lps_per_pe = calloc(NUM_PE, sizeof(int));
    read_lps_per_pe(rfile, lps_per_pe);

    if (args.filetype == GVT)
        sprintf(tmpname, "%sgvt", prefix);
    if (args.filetype == RT)
        sprintf(tmpname, "%srt", prefix);
    if (args.filetype == EVENT)
        sprintf(tmpname, "%sevrb", prefix);
    sprintf(filename, "%s-pes.csv",tmpname);
    output = fopen(filename, "w");
    sprintf(filename, "%s-lps.csv",tmpname);
    lp_out = fopen(filename, "w");
    sprintf(filename, "%s-kps.csv",tmpname);
    kp_out = fopen(filename, "w");

    if (args.filetype == GVT && !args.granularity)
        gvt_read(file, output);
    if (args.filetype == GVT && args.granularity)
        gvt_read_lps(file, output, lp_out, kp_out);
    if (args.filetype == RT && !args.granularity)
        rt_read(file, output);
    if (args.filetype == RT && args.granularity)
        rt_read_lps(file, output, lp_out, kp_out);
    if (args.filetype == EVENT)
        event_read(file, output);

    fclose(file);
    fclose(output);
    fclose(lp_out);
    fclose(kp_out);
    fclose(rfile);

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
        case 'p':
            NUM_PE = atoi(arg);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

char *get_prefix(char *filename)
{
    char *str, *token;
    char *saveptr;
    char delim[1] = {'-'};
    int i, idx = 0, end = 0;
    char *readme_file;
    readme_file = calloc(512, sizeof(char));

    for (i = 0, str=filename; ; i++, str=NULL)
    {
        token = strtok_r(str, delim, &saveptr);
        if (token == NULL)
            break;
        if (strcmp(saveptr, "gvt.bin") == 0 || strcmp(saveptr, "rt.bin") == 0 || strcmp(saveptr, "evrb.bin") == 0)
            end = 1;

        strcpy(&readme_file[idx], token);
        idx += saveptr-token;
        strcpy(&readme_file[idx-1], delim);
        if (end)
        {
            //strncpy(&readme_file[idx], "README.txt", 11);
            break;
        }

    }
    return readme_file;
}

void read_lps_per_pe(FILE *file, int *lps_per_pe)
{
    char *line = NULL;
    size_t n = 0;
    size_t read;
    int flag = 0;
    char *str, *token, *saveptr;
    char delim[1] = {','};
    int i;

    while ((read = getline(&line, &n, file)) != -1)
    {
        if (flag)
        {
            for (i = 0, str=line; ; i++, str=NULL)
            {
                token = strtok_r(str, delim, &saveptr);
                if (token == NULL)
                    break;
                lps_per_pe[i] = atoi(token);
            }

        }
        if (strcmp(line, "LPs per PE\n") == 0)
            flag = 1;
    }
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

void gvt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out)
{
    int i,j;
    int max_lps = lps_per_pe[0];
    int min_lps = lps_per_pe[NUM_PE-1];
    unsigned short id;
    //fprintf(output, "PE_ID,GVT,all_reduce_count,events_aborted,event_ties,fossil_collects,net_events,efficiency,");
    /*
     *for (i = 0; i < NUM_KP; i++)
     *    fprintf(output, "KP-%d_total_rollbacks,KP-%d_secondary_rollbacks,", i, i);
     *for (i = 0; i < max_lps; i++)
     *    fprintf(output, "LP-%d_events_processed,LP-%d_events_rolled_back,LP-%d_remote_sends,LP-%d_remote_recvs,", i, i, i, i);
     *for (i = 0; i < max_lps; i++)
     *{
     *    if (i == max_lps - 1)
     *        fprintf(output, "LP-%d_remote_events\n", i);
     *    else
     *        fprintf(output, "LP-%d_remote_events,", i);
     *}
     */
    fprintf(output, "PE_ID,GVT,all_reduce_count,events_aborted,event_ties,fossil_collects,net_events,efficiency\n");
    fprintf(lp_out, "LP_ID,PE_ID,KP_ID,GVT,events_processed,events_rolled_back,remote_sends,remote_recvs,remote_events\n");
    fprintf(kp_out, "KP_ID,PE_ID,GVT,total_rollbacks,secondary_rollbacks\n");

    gvt_line_lps myline_max, myline_min;
    myline_max.lp_vals = calloc(max_lps, sizeof(unsigned int*));
    myline_max.nsend_net_remote = calloc(max_lps, sizeof(int));
    myline_min.lp_vals = calloc(min_lps, sizeof(unsigned int*));
    myline_min.nsend_net_remote = calloc(min_lps, sizeof(int));
    for (i = 0; i < max_lps; i++)
        myline_max.lp_vals[i] = calloc(4, sizeof(unsigned int));
    for (i = 0; i < min_lps; i++)
        myline_min.lp_vals[i] = calloc(4, sizeof(unsigned int));

    while (!feof(file))
    {
        fread(&id, sizeof(unsigned short), 1, file);
        if (lps_per_pe[id] == max_lps)
        {
            myline_max.id = id;
            fread(&myline_max.ts, sizeof(myline_max.ts), 1, file);
            fread(&myline_max.values[0], sizeof(unsigned int), 4, file);
            fread(&myline_max.net_events, sizeof(myline_max.net_events), 1, file);
            fread(&myline_max.efficiency, sizeof(myline_max.efficiency), 1, file);
            fread(&myline_max.kp_vals[0][0], sizeof(unsigned int), NUM_KP * 2, file);
            for (i = 0; i < max_lps; i++)
            {
                for (j = 0; j < 4; j++)
                    fread(&myline_max.lp_vals[i][j], sizeof(unsigned int), 1, file);
            }
            fread(&myline_max.nsend_net_remote[0], sizeof(unsigned int), max_lps, file);
            print_gvt_lps_struct(output, lp_out, kp_out, &myline_max, max_lps);
        }
        else
        {
            myline_min.id = id;
            fread(&myline_min.ts, sizeof(myline_min.ts), 1, file);
            fread(&myline_min.values[0], sizeof(unsigned int), 4, file);
            fread(&myline_min.net_events, sizeof(myline_min.net_events), 1, file);
            fread(&myline_min.efficiency, sizeof(myline_min.efficiency), 1, file);
            fread(&myline_min.kp_vals[0][0], sizeof(unsigned int), NUM_KP * 2, file);
            for (i = 0; i < min_lps; i++)
            {
                for (j = 0; j < 4; j++)
                    fread(&myline_min.lp_vals[i][j], sizeof(unsigned int), 1, file);
            }
            fread(&myline_min.nsend_net_remote[0], sizeof(unsigned int), min_lps, file);
            print_gvt_lps_struct(output, lp_out, kp_out, &myline_min, min_lps);
        }
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

void rt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out)
{
    int i,j;
    int max_lps = lps_per_pe[0];
    int min_lps = lps_per_pe[NUM_PE-1];
    unsigned short id;
    /*
     *fprintf(output, "PE_ID,real_TS,current_GVT,");
     *for (i = 0; i < NUM_KP; i++)
     *    fprintf(output, "KP-%d_time_ahead_GVT,", i);
     *fprintf(output, "network_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancelq_CC,"
     *        "avl_CC,buddy_CC,lz4_CC,aborted_events,pq_size,event_ties,fossil_collect_attmepts,num_GVT_comps,");
     *for (i = 0; i < NUM_KP; i++)
     *    fprintf(output, "KP-%d_total_rollbacks,KP-%d_secondary_rollbacks,", i, i);
     *for (i = 0; i < max_lps; i++)
     *    fprintf(output, "LP-%d_events_processed,LP-%d_events_rolled_back,LP-%d_remote_sends,LP-%d_remote_recvs,", i, i, i, i);
     *for (i = 0; i < max_lps; i++)
     *{
     *    if (i == max_lps - 1)
     *        fprintf(output, "LP-%d_remote_events\n", i);
     *    else
     *        fprintf(output, "LP-%d_remote_events,", i);
     *}
     */

    fprintf(output, "PE_ID,real_TS,current_GVT,network_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancelq_CC,avl_CC,buddy_CC,lz4_CC,aborted_events,pq_size,event_ties,fossil_collect_attempts,num_GVT_comps\n");
    fprintf(lp_out, "LP_ID,KP_ID,PE_ID,real_TS,current_GVT,events_processed,events_rolled_back,remote_sends,remote_recvs,remote_events\n");
    fprintf(kp_out, "KP_ID,PE_ID,real_TS,current_GVT,time_ahead_GVT,total_rollbacks,secondary_rollbacks\n");

    rt_line_lps myline_max, myline_min;
    myline_max.lp_counters = calloc(max_lps, sizeof(unsigned int*));
    myline_max.nsend_net_remote = calloc(max_lps, sizeof(int));
    myline_min.lp_counters = calloc(min_lps, sizeof(unsigned int*));
    myline_min.nsend_net_remote = calloc(min_lps, sizeof(int));
    for (i = 0; i < max_lps; i++)
        myline_max.lp_counters[i] = calloc(4, sizeof(unsigned int));
    for (i = 0; i < min_lps; i++)
        myline_min.lp_counters[i] = calloc(4, sizeof(unsigned int));

    //fread(&myline, sizeof(rt_line_lps), 1, file);
    while (!feof(file))
    {
        //print_rt_lps_struct(output, &myline);
        //fread(&myline, sizeof(rt_line_lps), 1, file);
        fread(&id, sizeof(unsigned short), 1, file);
        if (lps_per_pe[id] == max_lps)
        {
            myline_max.id = id;
            fread(&myline_max.ts, sizeof(myline_max.ts), 1, file);
            fread(&myline_max.gvt, sizeof(myline_max.gvt), 1, file);
            fread(&myline_max.time_ahead_gvt[0], sizeof(float), NUM_KP, file);
            fread(&myline_max.cycles[0], sizeof(tw_clock), NUM_CYCLE_CTRS, file);
            fread(&myline_max.ev_counters[0], sizeof(unsigned int), 5, file);
            fread(&myline_max.kp_counters[0][0], sizeof(unsigned int), NUM_KP * 2, file);
            for (i = 0; i < max_lps; i++)
            {
                for (j = 0; j < 4; j++)
                    fread(&myline_max.lp_counters[i][j], sizeof(unsigned int), 1, file);
            }
            fread(&myline_max.nsend_net_remote[0], sizeof(unsigned int), max_lps, file);
            print_rt_lps_struct(output, lp_out, kp_out, &myline_max, max_lps);
        }
        else
        {
            myline_min.id = id;
            fread(&myline_min.ts, sizeof(myline_min.ts), 1, file);
            fread(&myline_min.gvt, sizeof(myline_min.gvt), 1, file);
            fread(&myline_min.time_ahead_gvt[0], sizeof(float), NUM_KP, file);
            fread(&myline_min.cycles[0], sizeof(tw_clock), NUM_CYCLE_CTRS, file);
            fread(&myline_min.ev_counters[0], sizeof(unsigned int), 5, file);
            fread(&myline_min.kp_counters[0][0], sizeof(unsigned int), NUM_KP * 2, file);
            for (i = 0; i < min_lps; i++)
            {
                for (j = 0; j < 4; j++)
                    fread(&myline_min.lp_counters[i][j], sizeof(unsigned int), 1, file);
            }
            fread(&myline_min.nsend_net_remote[0], sizeof(unsigned int), min_lps, file);
            print_rt_lps_struct(output, lp_out, kp_out, &myline_min, min_lps);
        }
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

void print_gvt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, gvt_line_lps *line, int num_lps)
{
    int i,j;
    int kp_id;

    fprintf(output, "%u,%f,", line->id, line->ts);
    for (i = 0; i < 4; i++)
        fprintf(output, "%u,", line->values[i]);
    fprintf(output, "%d,%f\n", line->net_events, line->efficiency);

    for (i = 0; i < NUM_KP; i++)
    {
        fprintf(kp_out, "%d,%u,%f", (line->id *NUM_KP + i), line->id, line->ts);
        for (j = 0; j < 2; j++)
            fprintf(kp_out, ",%u", line->kp_vals[i][j]);
        fprintf(kp_out, "\n");
    }

    for (i = 0; i < num_lps; i++)
    {
        kp_id = i % NUM_KP;
        fprintf(lp_out, "%d,%d,%u,%f", (line->id * lps_per_pe[line->id] + i), (line->id *NUM_KP + kp_id), line->id, line->ts);
        for (j = 0; j < 4; j++)
            fprintf(lp_out, ",%u", line->lp_vals[i][j]);
        fprintf(lp_out, ",%d\n", line->nsend_net_remote[i]);
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

void print_rt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, rt_line_lps *line, int num_lps)
{
    int i,j;
    int kp_id;

    fprintf(output, "%u,%f,%f", line->id, line->ts, line->gvt);
    for (i = 0; i < NUM_CYCLE_CTRS; i++)
#ifdef BGQ
        fprintf(output, ",%u", line->cycles[i]);
#else
        fprintf(output, ",%"PRIu64, line->cycles[i]);
#endif
    for (i = 0; i < 5; i++)
        fprintf(output, ",%u", line->ev_counters[i]);
    fprintf(output, "\n");

    for (i = 0; i < NUM_KP; i++)
    {
        fprintf(kp_out, "%d,%u,%f,%f,%f", (line->id *NUM_KP + i), line->id, line->ts, line->gvt, line->time_ahead_gvt[i]);
        for (j = 0; j < 2; j++)
            fprintf(kp_out, ",%u", line->kp_counters[i][j]);
        fprintf(kp_out, "\n");
    }

    for (i = 0; i < num_lps; i++)
    {
        kp_id = i % NUM_KP;
        fprintf(lp_out, "%d,%d,%u,%f,%f", (line->id * lps_per_pe[line->id] + i), (line->id *NUM_KP + kp_id), line->id, line->ts, line->gvt);
        for (j = 0; j < 4; j++)
            fprintf(lp_out, ",%u", line->lp_counters[i][j]);
        fprintf(lp_out, ",%d\n", line->nsend_net_remote[i]);
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

void print_event_struct(FILE *output, event_line *line)
{
    fprintf(output, "%"PRIu64",%"PRIu64",%f,%f,%d\n", line->src_lp, line->dest_lp, line->recv_ts_vt, line->recv_ts_rt, line->event_type);
}
