#include <stdio.h>
#include <unistd.h>
#include <oncillamem.h>

#define ALLOC_SIZE (1UL << 20)

int main(void)
{
    ocm_alloc_t a;
    void *buf;
    size_t buf_len, remote_len;

    if (0 > ocm_init()) {
        printf("Cannot connect to OCM\n");
        return -1;
    }

    a = ocm_alloc(ALLOC_SIZE, OCM_REMOTE_RDMA);
    if (!a) {
        printf("ocm_alloc failed on size %lu\n", ALLOC_SIZE);
        goto fail;
    }

    if (ocm_localbuf(a, &buf, &buf_len)) {
        printf("ocm_localbuf failed\n");
        goto fail;
    }
    printf("local buffer size %lu @ %p\n", buf_len, buf);

    if (ocm_is_remote(a)) {
        if (!ocm_remote_sz(a, &remote_len)) {
            printf("alloc is remote; size = %lu\n", remote_len);
        } else {
            printf("alloc is local\n");
        }
    }

#if 0
    if (ocm_free(a) < 0) {
        printf("ocm_free failed\n");
        return -1;
    }
#endif

    if (0 > ocm_tini()) {
        printf("ocm_tini failed\n");
        return -1;
    }

    return 0;

fail:
    if (0 > ocm_tini())
        printf("ocm_tini failed\n");
    return -1;
}
