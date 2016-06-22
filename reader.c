#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GVT 0
#define RT 1
#define NUM_GVT_VALS 11

typedef double tw_stime;
typedef unsigned long tw_peid;
typedef unsigned long long tw_stat;
typedef long tw_node;

typedef struct {
    tw_node id;
    tw_stime ts;
    tw_stat values[NUM_GVT_VALS];
    long long nsend_net_remote;
    long long net_events;
    double efficiency;
} gvt_line;

void gvt_read(FILE *file, int num_kps);
void rt_read(FILE *file, int num_kps);
void print_gvt_struct(gvt_line *line);

int main(int argc, char **argv) 
{
    FILE *file;
    file = fopen(argv[1], "r");

    int num_kps = atoi(argv[2]);
    int file_type = atoi(argv[3]); /* 0: GVT, 1: rt sampling  */
    if (file_type == GVT)
        gvt_read(file, num_kps);

    fclose(file);
}

void gvt_read(FILE *file, int num_kps)
{
    int num_bytes_per_block = sizeof(tw_stat) * NUM_GVT_VALS + sizeof(tw_node) + sizeof(tw_stime) + sizeof(double) + sizeof(long long) * 2;
    tw_node id;
    tw_stime gvt;
    double efficiency;
    tw_stat values[NUM_GVT_VALS];
    char data[num_bytes_per_block];
    gvt_line myline;
    int rc, index = 0;

    while (!feof(file))
    {
        rc = fread(&myline, sizeof(gvt_line), 1, file);
        print_gvt_struct(&myline);
    }
}

void rt_read(FILE *file, int num_kps)
{
}

void print_gvt_struct(gvt_line *line)
{
    int i;
    printf("%ld,%f,", line->id, line->ts);
    for (i = 0; i < NUM_GVT_VALS; i++)
        printf("%llu,", line->values[i]);
    printf("%lld,%lld,%f\n", line->nsend_net_remote, line->net_events, line->efficiency);
}
