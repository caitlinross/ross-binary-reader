#include <stdio.h>
#include <stdlib.h>

typedef double tw_stime;
typedef unsigned long tw_peid;

typedef struct {
    tw_peid id;
    tw_stime ts;
    tw_stime *values;
} line;

int main(int argc, char **argv) 
{
    FILE *file;
    file = fopen(argv[1], "r");

    int num_kps = atoi(argv[2]);
    int num_bytes_per_block = (num_kps + 1) * sizeof(tw_stime) + sizeof(tw_peid);
    tw_peid id;
    tw_stime values[num_kps + 1];
    char data[num_bytes_per_block + 1];
    line myline;
    myline.values = calloc(num_kps, sizeof(tw_stime));
    int rc;

    while (!feof(file))
    {
        rc = fread(&id, sizeof(tw_peid), 1, file);
        rc = fread(&values, sizeof(tw_stime), num_kps + 1, file);
        printf("%lu,", id);
        int i;
        for (i = 0; i < num_kps + 1; i++)
        {
            if (i != num_kps)
                printf("%f,", values[i]);
            else
                printf("%f\n", values[i]);
        }
    }
}
