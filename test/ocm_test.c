#include <stdio.h>
#include <unistd.h>
#include <oncillamem.h>

#define ALLOC_SIZE (1UL << 20)

int main(void)
{
    void *alloc = NULL;

    if (0 > ocm_init()) {
        printf("Cannot connect to OCM\n");
        return -1;
    }

    if (NULL == (alloc = ocm_alloc(ALLOC_SIZE, OCM_REMOTE_RDMA))) {
        printf("ocm_alloc failed on size %lu\n", ALLOC_SIZE);
        if (0 > ocm_tini())
            printf("ocm_tini failed\n");
        return -1;
    }

    printf("ocm_alloc returned %p\n", alloc);
    printf("local buffer is %p\n", ocm_localbuf(alloc));

#if 0
    if (ocm_free(alloc) < 0) {
        printf("ocm_free failed\n");
        return -1;
    }
#endif

    if (0 > ocm_tini()) {
        printf("ocm_tini failed\n");
        return -1;
    }

    return 0;
}
