/**
 * file: nodefile.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: node information
 */

/* System includes */
#include <string.h>

/* Other project includes */

/* Project includes */
#include <debug.h>
#include <nodefile.h>

/* Directory includes */

/* Globals */
struct node_entry *node_file = NULL; /* idx is rank */
int node_file_entries = 0;

/* Internal definitions */

/* Internal state */

/* Private functions */

/* Public functions */

int
parse_nodefile(const char *path, int *_myrank /* out */)
{
    int entries = 0;
    char *buf = NULL;
    const int buf_len = 256 + HOST_NAME_MAX;
    FILE *file = NULL;
    struct node_entry *e;
    int ret = -1, rank;

    if (!path)
        goto out;
    if (!(file = fopen(path, "r")))
        goto out;
    if (!(buf = calloc(1, buf_len)))
        goto out;

    while (fgets(buf, buf_len, file))
        if (*buf != '#')
            entries++;
    fseek(file, 0, 0);
    node_file = calloc(entries, sizeof(*node_file));
    if (!node_file)
        goto out;

    while (fgets(buf, buf_len, file)) {
        if (*buf == '#')
            continue;
        sscanf(buf, "%d", &rank);
        if (rank > entries - 1)
            goto out;
        printd("parsing %s", buf);
        e = &node_file[rank];
        /* XXX use strtok since e->dns and e->ip_eth could overflow */
        /* http://docs.roxen.com/pike/7.0/tutorial/strings/sscanf.xml */
        sscanf(buf, "%*d %s %s %d %d",
                e->dns, e->ip_eth, &e->ocm_port, &e->rdmacm_port);
    }

    if (gethostname(buf, HOST_NAME_MAX))
        goto out;
    rank = entries;
    while (rank-- > 0)
        if (0 == strncmp(node_file[rank].dns, buf, HOST_NAME_MAX))
            break;
    if (rank < 0)
    {
	printf("Couldn't find hostname listed in file on accessible systems\n");
        goto out;
    }
    *_myrank = rank;

    node_file_entries = entries;
    ret = 0;

out:
    if (file)
        fclose(file);
    if (buf)
        free(buf);
    if (ret < 0 && node_file)
        free(node_file);
    return ret;
}
