#include <stdio.h>
#include <oncillamem.h>

#define ALLOC_SIZE (1UL << 20)

int main(void)
{
    void *alloc = NULL;

    if (0 > ocm_init()) {
        printf("ocm_init failed\n");
        return -1;
    }

    if (NULL == (alloc = ocm_alloc(ALLOC_SIZE))) {
        printf("ocm_alloc failed on size %lu\n", ALLOC_SIZE);
        if (0 > ocm_tini())
            printf("ocm_tini failed\n");
        return -1;
    }

    printf("ocm_alloc returned %p\n", alloc);

    if (0 > ocm_tini()) {
        printf("ocm_tini failed\n");
        return -1;
    }

    return 0;
}
