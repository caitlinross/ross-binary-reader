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

#define MAX_LEN 2048

static char doc[] = "Reader for the binary data collection files from ROSS";
static char args_doc[] = "";
static struct argp_option options[] = {
    {"filename",  'f',  "str",  0,  "path of file to read" },
    {"filetype", 't',  "n",  0,  "type of file to read; 0: GVT data, 1: real time data, 2: event data" },
    {"combine-events", 'c', 0, 0, "bin event trace to reduce data size"},
    {"bin-size", 'b', "n", 0, "amount of virtual time in bin for binning event trace (default: 10000)"},
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
    unsigned int *values;
    int nsend_net_remote;
    int net_events;
    float efficiency;
} __attribute__((__packed__));

typedef struct gvt_line_lps gvt_line_lps;
struct gvt_line_lps{
    unsigned short id;
    float ts;
    unsigned int *values;
    float efficiency;
    unsigned int **kp_vals;
    unsigned int **lp_vals;
    int *nsend_net_remote;
} __attribute__((__packed__));

typedef struct rt_line rt_line;
struct rt_line{
    unsigned short id;
    float ts;
    float gvt;
    float *time_ahead_gvt;
    tw_clock *cycles;
    unsigned int *ev_counters;
} __attribute__((__packed__));

typedef struct rt_line_lps rt_line_lps;
struct rt_line_lps {
    unsigned short id;
    float ts;
    float gvt;
    float *time_ahead_gvt;
    tw_clock *cycles;
    unsigned int *ev_counters;
    unsigned int **kp_counters;
    unsigned int **lp_counters;
    int *nsend_net_remote;
} __attribute__((__packed__));

typedef struct event_line event_line;
struct event_line{
    tw_lpid src_lp;
    tw_lpid dest_lp;
    tw_stime recv_ts_vt;
    tw_stime recv_ts_rt;
    int event_type;
} __attribute__((__packed__));

typedef struct event_bin event_bin;
struct event_bin{
    tw_stime recv_ts_vt;
    unsigned long long **event_count;
    event_bin *next;
} __attribute__((__packed__));

typedef enum {
    GVT,
    RT,
    EVENT
} file_types;

int g_granularity = 0;
int g_num_pe = 0;
int g_num_kp = 0;
int *g_lps_per_pe;
int *g_total_lp_offsets;
int g_num_gvt_vals_pe = 0;
int g_num_gvt_vals_kp = 0;
int g_num_gvt_vals_lp = 0;
int g_num_cycle_ctrs = 0;
int g_num_ev_ctrs_pe = 0;
int g_num_ev_ctrs_kp = 0;
int g_num_ev_ctrs_lp = 0;
int g_combine = 0;
int g_num_bins = 0;
int g_bin_size = 10000;
int g_total_lps = 0;
event_bin *g_bin_list;
event_bin *g_cur_bin;
event_bin *g_last_bin;

static error_t parse_opt (int key, char *arg, struct argp_state *state);
void read_metadata(char *path, char *prefix);
void gvt_read(FILE *file, FILE *output);
void gvt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out);
void rt_read(FILE *file, FILE *output, FILE *kp_out);
void rt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out);
void event_read(FILE *file, FILE *output);
void print_gvt_struct(FILE *output, gvt_line *line);
void print_gvt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, gvt_line_lps *line, int num_lps);
void print_rt_struct(FILE *output, FILE *kp_out, rt_line *line);
void print_rt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, rt_line_lps *line, int num_lps);
void print_event_struct(FILE *output, event_line *line);
char *get_prefix(char *filename, char *path);
void bin_event(event_line *line);
void print_binned_events(FILE *output);

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv)
{
    FILE *file = NULL, *output = NULL, *lp_out = NULL, *kp_out = NULL;
    char filename[MAX_LEN];
    char tmpname[MAX_LEN];
    struct arguments args = {0};
    int i;
    char *prefix;
    char path[MAX_LEN];

    /* Default values. */
    args.filename = "-";
    args.filetype = 0;

    /* Parse our arguments; every option seen by parse_opt will
     *      be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &args);

    file = fopen(args.filename, "r");
    prefix = get_prefix(args.filename, path);
    read_metadata(path, prefix);

    // set up for binning event trace while reading
    if (g_combine)
    {
        g_num_bins++;
        g_bin_list = calloc(1, sizeof(event_bin));
        g_bin_list->event_count = calloc(g_total_lps, sizeof(unsigned long long*));
        for (i = 0; i < g_total_lps; i++)
            g_bin_list->event_count[i] = calloc(g_total_lps, sizeof(unsigned long long));
        g_bin_list->next = NULL;
        g_last_bin = g_bin_list;
        g_cur_bin = g_bin_list;
    }

    // open the necessary files for output
    if (args.filetype == GVT)
        sprintf(tmpname, "%s%sgvt", path, prefix);
    else if (args.filetype == RT)
        sprintf(tmpname, "%s%srt", path, prefix);
    else if (args.filetype == EVENT)
    {
        sprintf(filename, "%s%sevtrace.csv", path, prefix);
        output = fopen(filename, "w");
    }
    if (args.filetype != EVENT)
    {
        sprintf(filename, "%s-pes.csv",tmpname);
        output = fopen(filename, "w");
        if (g_granularity || args.filetype == RT)
        {
            sprintf(filename, "%s-kps.csv",tmpname);
            kp_out = fopen(filename, "w");
        }
        if (g_granularity)
        {
            sprintf(filename, "%s-lps.csv",tmpname);
            lp_out = fopen(filename, "w");
        }
    }

    // call the correct function to read the binary file
    if (args.filetype == GVT && !g_granularity)
        gvt_read(file, output);
    if (args.filetype == GVT && g_granularity)
        gvt_read_lps(file, output, lp_out, kp_out);
    if (args.filetype == RT && !g_granularity)
        rt_read(file, output, kp_out);
    if (args.filetype == RT && g_granularity)
        rt_read_lps(file, output, lp_out, kp_out);
    if (args.filetype == EVENT)
        event_read(file, output);

    if (file)
        fclose(file);
    if (output)
        fclose(output);
    if (kp_out)
        fclose(kp_out);
    if (lp_out)
        fclose(lp_out);

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
        case 'c':
            g_combine = 1;
            break;
        case 'b':
            g_bin_size = atoi(arg);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void read_metadata(char *path, char *prefix)
{
    FILE *file;
    char readme_file[MAX_LEN];
    char *line = NULL;
    size_t n = 0;
    size_t read;
    char *str, *token, *saveptr;
    char delim[] = "=";
    char lpdelim[] = ",";
    int i;

    sprintf(readme_file, "%s%sREADME.txt", path, prefix);
    file = fopen(readme_file, "r");

    while ((read = getline(&line, &n, file)) != -1)
    {
        line[read-1] = '\0';
        if (strncmp(line, "GRANULARITY", 11)== 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_granularity = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_PE", 6) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_pe = atoi(saveptr);
            g_lps_per_pe = calloc(g_num_pe, sizeof(int));
        }
        else if (strncmp(line, "NUM_KP", 6) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_kp = atoi(saveptr);
        }
        else if (strncmp(line, "LP_PER_PE", 9) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            for (i = 0, str=saveptr; ; i++, str=NULL)
            {
                token = strtok_r(str, lpdelim, &saveptr);
                if (token == NULL)
                    break;
                g_lps_per_pe[i] = atoi(token);
            }

        }
        else if (strncmp(line, "NUM_GVT_VALS_PE", 15) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_gvt_vals_pe = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_GVT_VALS_KP", 15) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_gvt_vals_kp = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_GVT_VALS_LP", 15) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_gvt_vals_lp = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_CYCLE_CTRS", 14) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_cycle_ctrs = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_EV_CTRS_PE", 14) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_ev_ctrs_pe = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_EV_CTRS_KP", 14) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_ev_ctrs_kp = atoi(saveptr);
        }
        else if (strncmp(line, "NUM_EV_CTRS_LP", 14) == 0)
        {
            str = line;
            token = strtok_r(str, delim, &saveptr);
            g_num_ev_ctrs_lp = atoi(saveptr);
        }
    }

    // get offsets for determining global LP IDs
    if (g_lps_per_pe)
    {
        g_total_lp_offsets = calloc(g_num_pe, sizeof(int));
        g_total_lp_offsets[0] = 0;
        g_total_lps = g_lps_per_pe[0];
        for (i = 1; i < g_num_pe; i++)
        {
            g_total_lp_offsets[i] = g_total_lp_offsets[i-1] + g_lps_per_pe[i-1];
            g_total_lps += g_lps_per_pe[i];
        }
    }
    else
    {
        fprintf(stderr, "ERROR: was not able to read LP_PER_PE from README!\n");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

char *get_prefix(char *filename, char *path)
{
    char *str, *token;
    char *saveptr;
    char fileroot[MAX_LEN];
    char pathdelim[2] = {'/', '\0'};
    char delim[2] = {'-', '\0'};
    int i, idx = 0, end = 0;
    char *readme_file;
    readme_file = calloc(MAX_LEN, sizeof(char));

    for (i = 0, str=filename; ; i++, str=NULL)
    {
        token = strtok_r(str, pathdelim, &saveptr);
        if (token == NULL)
            break;
        if (strcmp(saveptr, "") != 0)
        {
            strcpy(&path[idx], token);
            idx += saveptr-token;
            strcpy(&path[idx-1], pathdelim);
        }

        strcpy(fileroot, token);
    }
    idx = 0;
    for (i = 0, str=fileroot; ; i++, str=NULL)
    {
        token = strtok_r(str, delim, &saveptr);
        if (token == NULL)
            break;
        if (strcmp(saveptr, "gvt.bin") == 0 || strcmp(saveptr, "rt.bin") == 0 || strcmp(saveptr, "evtrace.bin") == 0)
            end = 1;

        strcpy(&readme_file[idx], token);
        idx += saveptr-token;
        strcpy(&readme_file[idx-1], delim);
        if (end)
            break;

    }
    return readme_file;
}

void gvt_read(FILE *file, FILE *output)
{
    gvt_line myline;
    myline.values = calloc(g_num_gvt_vals_pe, sizeof(unsigned int));
    fprintf(output, "PE_ID,GVT,all_reduce_count,events_processed,events_aborted,events_rolled_back,event_ties,total_rollbacks,primary_rollbacks,"
            "secondary_rollbacks,fossil_collects,network_sends,network_recvs,remote_events,net_events,efficiency\n");
    while (!feof(file))
    {
        fread(&myline.id, sizeof(myline.id), 1, file);
        fread(&myline.ts, sizeof(myline.ts), 1, file);
        fread(&myline.values[0], sizeof(unsigned int), g_num_gvt_vals_pe, file);
        fread(&myline.nsend_net_remote, sizeof(myline.nsend_net_remote), 1, file);
        fread(&myline.net_events, sizeof(myline.net_events), 1, file);
        fread(&myline.efficiency, sizeof(myline.efficiency), 1, file);
        print_gvt_struct(output, &myline);
    }
}

void gvt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out)
{
    int i;
    int max_lps = g_lps_per_pe[0];
    int min_lps = g_lps_per_pe[g_num_pe-1];
    unsigned short id;
    fprintf(output, "PE_ID,GVT,all_reduce_count,events_aborted,event_ties,fossil_collects,efficiency\n");
    fprintf(lp_out, "LP_ID,KP_ID,PE_ID,GVT,events_processed,events_rolled_back,remote_sends,remote_recvs,remote_events\n");
    fprintf(kp_out, "KP_ID,PE_ID,GVT,total_rollbacks,secondary_rollbacks\n");

    gvt_line_lps myline_max, myline_min;
    myline_max.values = calloc(g_num_gvt_vals_pe, sizeof(unsigned int));
    myline_max.kp_vals = calloc(g_num_kp, sizeof(unsigned int*));
    for (i = 0; i < g_num_kp; i++)
        myline_max.kp_vals[i] = calloc(g_num_gvt_vals_kp, sizeof(unsigned int));

    myline_max.lp_vals = calloc(max_lps, sizeof(unsigned int*));
    for (i = 0; i < max_lps; i++)
        myline_max.lp_vals[i] = calloc(g_num_gvt_vals_lp, sizeof(unsigned int));

    myline_max.nsend_net_remote = calloc(max_lps, sizeof(int));

    myline_min.values = calloc(g_num_gvt_vals_pe, sizeof(unsigned int));
    myline_min.kp_vals = calloc(g_num_kp, sizeof(unsigned int*));
    for (i = 0; i < g_num_kp; i++)
        myline_min.kp_vals[i] = calloc(g_num_gvt_vals_kp, sizeof(unsigned int));

    myline_min.lp_vals = calloc(min_lps, sizeof(unsigned int*));
    for (i = 0; i < min_lps; i++)
        myline_min.lp_vals[i] = calloc(g_num_gvt_vals_lp, sizeof(unsigned int));

    myline_min.nsend_net_remote = calloc(min_lps, sizeof(int));

    while (!feof(file))
    {
        fread(&id, sizeof(unsigned short), 1, file);
        if (g_lps_per_pe[id] == max_lps)
        {
            myline_max.id = id;
            fread(&myline_max.ts, sizeof(myline_max.ts), 1, file);
            fread(&myline_max.values[0], sizeof(unsigned int), g_num_gvt_vals_pe, file);
            fread(&myline_max.efficiency, sizeof(myline_max.efficiency), 1, file);
            for (i = 0; i < g_num_kp; i++)
                fread(&myline_max.kp_vals[i][0], sizeof(unsigned int), g_num_gvt_vals_kp, file);
            for (i = 0; i < max_lps; i++)
                fread(&myline_max.lp_vals[i][0], sizeof(unsigned int), g_num_gvt_vals_lp, file);
            fread(&myline_max.nsend_net_remote[0], sizeof(unsigned int), max_lps, file);
            print_gvt_lps_struct(output, lp_out, kp_out, &myline_max, max_lps);
        }
        else
        {
            myline_min.id = id;
            fread(&myline_min.ts, sizeof(myline_min.ts), 1, file);
            fread(&myline_min.values[0], sizeof(unsigned int), g_num_gvt_vals_pe, file);
            fread(&myline_min.efficiency, sizeof(myline_min.efficiency), 1, file);
            for (i = 0; i < g_num_kp; i++)
                fread(&myline_min.kp_vals[i][0], sizeof(unsigned int), g_num_gvt_vals_kp, file);
            for (i = 0; i < min_lps; i++)
                fread(&myline_min.lp_vals[i][0], sizeof(unsigned int), g_num_gvt_vals_lp, file);
            fread(&myline_min.nsend_net_remote[0], sizeof(unsigned int), min_lps, file);
            print_gvt_lps_struct(output, lp_out, kp_out, &myline_min, min_lps);
        }
    }
}

void rt_read(FILE *file, FILE *output, FILE *kp_out)
{
    rt_line myline;
    myline.time_ahead_gvt = calloc(g_num_kp, sizeof(float));
    myline.cycles = calloc(g_num_cycle_ctrs, sizeof(tw_clock));
    myline.ev_counters = calloc(g_num_ev_ctrs_pe, sizeof(unsigned int));
    fprintf(output, "PE_ID,real_TS,current_GVT,");
    fprintf(output, "network_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancelq_CC,"
            "avl_CC,buddy_CC,lz4_CC,aborted_events,pq_size,remote_events,network_sends,network_recvs,"
            "event_ties,fossil_collect_attempts,num_GVTs,events_processed,events_rolled_back,total_rollbacks,secondary_rollbacks,net_events,"
            "primary_rb\n");
    fprintf(kp_out, "KP_ID,PE_ID,real_TS,current_GVT,time_ahead_GVT\n");
    while (!feof(file))
    {
        fread(&myline.id, sizeof(myline.id), 1, file);
        fread(&myline.ts, sizeof(myline.ts), 1, file);
        fread(&myline.gvt, sizeof(myline.gvt), 1, file);
        fread(&myline.time_ahead_gvt[0], sizeof(float), g_num_kp, file);
        fread(&myline.cycles[0], sizeof(tw_clock), g_num_cycle_ctrs, file);
        fread(&myline.ev_counters[0], sizeof(unsigned int), g_num_ev_ctrs_pe, file);
        print_rt_struct(output, kp_out, &myline);
    }

}

void rt_read_lps(FILE *file, FILE *output, FILE *lp_out, FILE *kp_out)
{
    int i;
    int max_lps = g_lps_per_pe[0];
    int min_lps = g_lps_per_pe[g_num_pe-1];
    unsigned short id;

    fprintf(output, "PE_ID,real_TS,current_GVT,network_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancelq_CC,avl_CC,buddy_CC,lz4_CC,aborted_events,pq_size,event_ties,fossil_collect_attempts,num_GVTs\n");
    fprintf(lp_out, "LP_ID,KP_ID,PE_ID,real_TS,current_GVT,events_processed,events_rolled_back,remote_sends,remote_recvs,remote_events\n");
    fprintf(kp_out, "KP_ID,PE_ID,real_TS,current_GVT,time_ahead_GVT,total_rollbacks,secondary_rollbacks\n");

    rt_line_lps myline_max, myline_min;
    myline_max.time_ahead_gvt = calloc(g_num_kp, sizeof(float));
    myline_max.cycles = calloc(g_num_cycle_ctrs, sizeof(tw_clock));
    myline_max.ev_counters = calloc(g_num_ev_ctrs_pe, sizeof(unsigned int));
    myline_max.kp_counters = calloc(g_num_kp, sizeof(unsigned int*));
    for (i = 0; i < g_num_kp; i++)
        myline_max.kp_counters[i] = calloc(g_num_ev_ctrs_kp, sizeof(unsigned int));
    myline_max.lp_counters = calloc(max_lps, sizeof(unsigned int*));
    for (i = 0; i < max_lps; i++)
        myline_max.lp_counters[i] = calloc(g_num_ev_ctrs_lp, sizeof(unsigned int));
    myline_max.nsend_net_remote = calloc(max_lps, sizeof(int));
    myline_min.time_ahead_gvt = calloc(g_num_kp, sizeof(float));
    myline_min.cycles = calloc(g_num_cycle_ctrs, sizeof(tw_clock));
    myline_min.ev_counters = calloc(g_num_ev_ctrs_pe, sizeof(unsigned int));
    myline_min.kp_counters = calloc(g_num_kp, sizeof(unsigned int*));
    for (i = 0; i < g_num_kp; i++)
        myline_min.kp_counters[i] = calloc(g_num_ev_ctrs_kp, sizeof(unsigned int));
    myline_min.lp_counters = calloc(min_lps, sizeof(unsigned int*));
    for (i = 0; i < min_lps; i++)
        myline_min.lp_counters[i] = calloc(4, sizeof(unsigned int));
    myline_min.nsend_net_remote = calloc(min_lps, sizeof(int));

    while (!feof(file))
    {
        fread(&id, sizeof(unsigned short), 1, file);
        if (g_lps_per_pe[id] == max_lps)
        {
            myline_max.id = id;
            fread(&myline_max.ts, sizeof(myline_max.ts), 1, file);
            fread(&myline_max.gvt, sizeof(myline_max.gvt), 1, file);
            fread(&myline_max.time_ahead_gvt[0], sizeof(float), g_num_kp, file);
            fread(&myline_max.cycles[0], sizeof(tw_clock), g_num_cycle_ctrs, file);
            fread(&myline_max.ev_counters[0], sizeof(unsigned int), g_num_ev_ctrs_pe, file);
            for (i = 0; i < g_num_kp; i++)
                fread(&myline_max.kp_counters[i][0], sizeof(unsigned int), g_num_ev_ctrs_kp, file);
            for (i = 0; i < max_lps; i++)
                fread(&myline_max.lp_counters[i][0], sizeof(unsigned int), g_num_ev_ctrs_lp, file);
            fread(&myline_max.nsend_net_remote[0], sizeof(unsigned int), max_lps, file);
            print_rt_lps_struct(output, lp_out, kp_out, &myline_max, max_lps);
        }
        else
        {
            myline_min.id = id;
            fread(&myline_min.ts, sizeof(myline_min.ts), 1, file);
            fread(&myline_min.gvt, sizeof(myline_min.gvt), 1, file);
            fread(&myline_min.time_ahead_gvt[0], sizeof(float), g_num_kp, file);
            fread(&myline_min.cycles[0], sizeof(tw_clock), g_num_cycle_ctrs, file);
            fread(&myline_min.ev_counters[0], sizeof(unsigned int), g_num_ev_ctrs_pe, file);
            for (i = 0; i < g_num_kp; i++)
                fread(&myline_min.kp_counters[i][0], sizeof(unsigned int), g_num_ev_ctrs_kp, file);
            for (i = 0; i < min_lps; i++)
                fread(&myline_min.lp_counters[i][0], sizeof(unsigned int), g_num_ev_ctrs_lp, file);
            fread(&myline_min.nsend_net_remote[0], sizeof(unsigned int), min_lps, file);
            print_rt_lps_struct(output, lp_out, kp_out, &myline_min, min_lps);
        }
    }
}

void event_read(FILE *file, FILE *output)
{
    event_line myline;
    if (g_combine)
        fprintf(output, "src_lp,dest_lp,recv_ts_vt,event_count\n");
    else
        fprintf(output, "src_lp,dest_lp,recv_ts_vt,recv_ts_rt,event_type\n");

    while (!feof(file))
    {
        fread(&myline, sizeof(event_line), 1, file);
        if (g_combine)
            bin_event(&myline);
        else
            print_event_struct(output, &myline);
    }
    if (g_combine)
        print_binned_events(output);
}

void bin_event(event_line *line)
{
    event_bin *this_bin;
    int i;
    while (line->recv_ts_vt >= g_last_bin->recv_ts_vt + g_bin_size)
    {
        // need to add a new bin
        g_num_bins++;
        event_bin *new_bin = calloc(1, sizeof(event_bin));
        new_bin->event_count = calloc(g_total_lps, sizeof(unsigned long long*));
        for (i = 0; i < g_total_lps; i++)
            new_bin->event_count[i] = calloc(g_total_lps, sizeof(unsigned long long));
        new_bin->recv_ts_vt = g_last_bin->recv_ts_vt + g_bin_size;
        new_bin->next = NULL;
        g_last_bin->next = new_bin;
        g_last_bin = new_bin;
    }

    // check if we need to reset g_cur_bin
    if (line->recv_ts_vt < g_cur_bin->recv_ts_vt)
        g_cur_bin = g_bin_list;

    // now loop through bins from g_cur_bin
    for (this_bin = g_cur_bin; this_bin != NULL; this_bin = this_bin->next)
    {
        if (line->recv_ts_vt >= this_bin->recv_ts_vt && line->recv_ts_vt < this_bin->recv_ts_vt + g_bin_size)
        {
            //found correct bin
            g_cur_bin = this_bin;
            (g_cur_bin->event_count[line->src_lp][line->dest_lp]) += 1;
            break;
        }
    }
}

void print_gvt_struct(FILE *output, gvt_line *line)
{
    int i;
    fprintf(output, "%u,%f,", line->id, line->ts);
    for (i = 0; i < g_num_gvt_vals_pe; i++)
        fprintf(output, "%u,", line->values[i]);
    fprintf(output, "%d,%d,%f\n", line->nsend_net_remote, line->net_events, line->efficiency);
}

void print_gvt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, gvt_line_lps *line, int num_lps)
{
    int i,j;
    int kp_id;

    fprintf(output, "%u,%f,", line->id, line->ts);
    for (i = 0; i < g_num_ev_ctrs_pe; i++)
        fprintf(output, "%u,", line->values[i]);
    fprintf(output, "%f\n", line->efficiency);

    for (i = 0; i < g_num_kp; i++)
    {
        fprintf(kp_out, "%d,%u,%f", (line->id *g_num_kp + i), line->id, line->ts);
        for (j = 0; j < g_num_gvt_vals_kp; j++)
            fprintf(kp_out, ",%u", line->kp_vals[i][j]);
        fprintf(kp_out, "\n");
    }

    for (i = 0; i < num_lps; i++)
    {
        kp_id = i % g_num_kp;
        fprintf(lp_out, "%d,%d,%u,%f", (g_total_lp_offsets[line->id] + i), (line->id *g_num_kp + kp_id), line->id, line->ts);
        for (j = 0; j < g_num_gvt_vals_lp; j++)
            fprintf(lp_out, ",%u", line->lp_vals[i][j]);
        fprintf(lp_out, ",%d\n", line->nsend_net_remote[i]);
    }
}

void print_rt_struct(FILE *output, FILE *kp_out, rt_line *line)
{
    int i;
    fprintf(output, "%u,%f,%f", line->id, line->ts, line->gvt);
    for (i = 0; i < g_num_cycle_ctrs; i++)
#ifdef BGQ
        fprintf(output, ",%u", line->cycles[i]);
#else
        fprintf(output, ",%"PRIu64, line->cycles[i]);
#endif
    for (i = 0; i < g_num_ev_ctrs_pe; i++)
        fprintf(output, ",%u", line->ev_counters[i]);
    fprintf(output, "\n");

    // time ahead of GVT always at KP granularity
    for (i = 0; i < g_num_kp; i++)
        fprintf(kp_out, "%d,%u,%f,%f,%f\n", (line->id *g_num_kp + i), line->id, line->ts, line->gvt, line->time_ahead_gvt[i]);
}

void print_rt_lps_struct(FILE *output, FILE *lp_out, FILE *kp_out, rt_line_lps *line, int num_lps)
{
    int i,j;
    int kp_id;

    fprintf(output, "%u,%f,%f", line->id, line->ts, line->gvt);
    for (i = 0; i < g_num_cycle_ctrs; i++)
#ifdef BGQ
        fprintf(output, ",%u", line->cycles[i]);
#else
        fprintf(output, ",%"PRIu64, line->cycles[i]);
#endif
    for (i = 0; i < g_num_ev_ctrs_pe; i++)
        fprintf(output, ",%u", line->ev_counters[i]);
    fprintf(output, "\n");

    for (i = 0; i < g_num_kp; i++)
    {
        fprintf(kp_out, "%d,%u,%f,%f,%f", (line->id *g_num_kp + i), line->id, line->ts, line->gvt, line->time_ahead_gvt[i]);
        for (j = 0; j < g_num_ev_ctrs_kp; j++)
            fprintf(kp_out, ",%u", line->kp_counters[i][j]);
        fprintf(kp_out, "\n");
    }

    for (i = 0; i < num_lps; i++)
    {
        kp_id = i % g_num_kp;
        fprintf(lp_out, "%d,%d,%u,%f,%f", (g_total_lp_offsets[line->id] + i), (line->id *g_num_kp + kp_id), line->id, line->ts, line->gvt);
        for (j = 0; j < g_num_ev_ctrs_lp; j++)
            fprintf(lp_out, ",%u", line->lp_counters[i][j]);
        fprintf(lp_out, ",%d\n", line->nsend_net_remote[i]);
    }
}

void print_event_struct(FILE *output, event_line *line)
{
    fprintf(output, "%"PRIu64",%"PRIu64",%f,%f,%d\n", line->src_lp, line->dest_lp, line->recv_ts_vt, line->recv_ts_rt, line->event_type);
}

void print_binned_events(FILE *output)
{
    int i, j;
    for (g_cur_bin = g_bin_list; g_cur_bin != NULL; g_cur_bin = g_cur_bin->next)
    {
        for (i = 0; i < g_total_lps; i++)
        {
            for (j = 0; j < g_total_lps; j++)
            {
                if (g_cur_bin->event_count[i][j] > 0)
                    fprintf(output, "%d,%d,%f,%llu\n", i, j, g_cur_bin->recv_ts_vt, g_cur_bin->event_count[i][j]);
            }
        }
    }
}
