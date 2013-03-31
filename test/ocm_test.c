#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <oncillamem.h>
#include <math.h>

int main(int argc, char *argv[])
{

  ocm_alloc_t a;
  void *buf;
  size_t buf_len, remote_len;
  ocm_alloc_param_t alloc_params;
  ocm_param_t copy_params;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <local_alloc_size_MB> <remote_alloc_size_MB>\n"
        "\talloc_size: can be specified in any positive decimal format\n", argv[0]);
    return -1;
  }

  //Convert the double values for MB input to bytes
  double local_size_MB = strtod(argv[1], 0);
  uint64_t local_size_B = (uint64_t)(local_size_MB*pow(2,20));

  //Convert the double values for MB input to bytes
  double rem_size_MB = strtod(argv[2], 0);
  uint64_t rem_size_B = (uint64_t)(rem_size_MB*pow(2,20));

  if(local_size_B > rem_size_B)
  {
    printf("Please use a larger remote buffer size than local size\n");
    return -1;
  }

  printf("Testing IB allocation with local buffer of size %4f MB and remote buffer of size %4f MB\n", local_size_MB, rem_size_MB);

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  //Create a structure that is used to configure the allocation on each endpoint
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  alloc_params->local_alloc_bytes = local_size_B;
  alloc_params->rem_alloc_bytes = rem_size_B;
  alloc_params->kind = OCM_REMOTE_RDMA;

  a = ocm_alloc(alloc_params);
  if (!a) {
    printf("ocm_alloc failed on remote size %lu\n", rem_size_B);
    return -1;
  }

  copy_params = (ocm_param_t)calloc(1,sizeof(struct ocm_params));
  copy_params->src_offset = 0;
  copy_params->dest_offset = 0;
  copy_params->bytes = local_size_B;
  copy_params->op_flag = 0;

  //Use a one-sided copy since we are copying from a local-remote IB
  //paired object
  if(ocm_copy_onesided(a, copy_params)){
    printf("ocm_copy_onesided (read) failed\n");
    goto fail;
  } 

  copy_params->op_flag = 1;

  if(ocm_copy_onesided(a, copy_params)){
    printf("ocm_copy_onesided (write) failed\n");
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

  free(alloc_params);
  free(copy_params);

  if (0 > ocm_tini()) {
    printf("ocm_tini failed\n");
    return -1;
  }

  printf("OCM test completed successfully\n");
  return 0;

fail:

  free(alloc_params);
  free(copy_params);
  if (0 > ocm_tini())
    printf("ocm_tini failed\n");
  return -1;
}
