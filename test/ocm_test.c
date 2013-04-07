#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <oncillamem.h>
#include <math.h>

//Needed to explicitly close EXTOLL connections
#ifdef EXTOLL
#include <io/extoll.h>
#include "../src/extoll.h"
#endif

#ifdef CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#endif


static int alloc_test(int suboption, uint64_t local_size_B, uint64_t rem_size_B){
  ocm_alloc_t a;
  void *buf;
  size_t buf_len, remote_len;
  ocm_alloc_param_t alloc_params;

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  //Create a structure that is used to configure the allocation on each endpoint
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  alloc_params->local_alloc_bytes = local_size_B;
  switch (suboption){
    case 1:
      alloc_params->kind = OCM_LOCAL_HOST;
      break;
    case 2:
      alloc_params->kind = OCM_LOCAL_GPU;
      break;
    case 3:
      alloc_params->kind = OCM_REMOTE_RDMA;
      alloc_params->rem_alloc_bytes = rem_size_B;
      break;
    case 4:
      alloc_params->kind = OCM_REMOTE_RMA;
      alloc_params->rem_alloc_bytes = rem_size_B;
      break;
    default:
      goto usage;
  }
      
  a = ocm_alloc(alloc_params);
  if (!a) {
    printf("ocm_alloc failed on remote size %lu\n", rem_size_B);
    return -1;
  }

  printf("Checking to see if ocm_localbuf works.\n");
  //Check to see if we can access a local buffer endpoint
  if (ocm_localbuf(a, &buf, &buf_len)) 
  {
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

  free(alloc_params);

  //**** For EXTOLL we must actively kill the client process
  //to avoid leaving any pinned pages around
  if(suboption == 4)
  {
    if(ocm_extoll_disconnect(a))
      goto fail;
  }

  if (0 > ocm_tini()) {
    printf("ocm_tini failed\n");
    return -1;
  }

  printf("OCM test completed successfully\n");
  return 0;

fail:

  free(alloc_params);
  if (0 > ocm_tini())
    printf("ocm_tini failed\n");
  return -1;

usage:
    fprintf(stderr, "Usage:  <which test> <test_suboption> <allocation size 1 in MB (alloc1)>"
        " <allocation size 2 in MB (alloc2)>\n "
	"\twhich test: 1=allocation; 2=copy-onesided; 3=copy-twosided\n" 
	"\tSuboptions: 1=allocate host memory; 2=allocate GPU memory; 3=allocate IB buffer (alloc1-local, alloc2-remote)\n"
	" \t\t 4=allocate EXTOLL buffer (alloc1-local, alloc2-remote)\n");
    return -1;
}

static int copy_onesided_test(uint64_t local_size_B, uint64_t rem_size_B){
  ocm_alloc_t a;
  void *buf;
  size_t buf_len, remote_len;
  ocm_alloc_param_t alloc_params;
  ocm_param_t copy_params;

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  //Create a structure that is used to configure the allocation on each endpoint
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  alloc_params->local_alloc_bytes = local_size_B;
  alloc_params->rem_alloc_bytes = rem_size_B;
  #ifdef INFINIBAND
  alloc_params->kind = OCM_REMOTE_RDMA;
  #endif
  #ifdef EXTOLL
  alloc_params->kind = OCM_REMOTE_RMA;
  #endif

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

static int copy_twosided_test(uint64_t local_size_B,uint64_t rem_size_B){
  ocm_alloc_t local_alloc, remote_alloc, local_alloc2;
  #ifdef CUDA
  ocm_alloc_t gpu_alloc;
  #endif
//  void *buf;
//  size_t buf_len, remote_len;
  ocm_alloc_param_t alloc_params;
  ocm_param_t copy_params;

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  // local allocation
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  alloc_params->local_alloc_bytes = local_size_B;
  alloc_params->kind = OCM_LOCAL_HOST;
 
  local_alloc = ocm_alloc(alloc_params);
  if (!local_alloc) {
    printf("local ocm_alloc failed on alloc size %lu\n", local_size_B);
    return -1;
  }
  printf("local alloc success\n");
  // local allocation 2
  alloc_params->kind = OCM_LOCAL_HOST;
 
  local_alloc2 = ocm_alloc(alloc_params);
  if (!local_alloc2) {
    printf("local ocm_alloc failed on alloc size %lu\n", local_size_B);
    return -1;
  }
  printf("second local alloc success\n");
  //GPU allocation
  alloc_params->kind = OCM_LOCAL_GPU;
/* 
  gpu_alloc = ocm_alloc(alloc_params);
  if (!gpu_alloc) {
    printf("gpu ocm_alloc failed on gpu alloc size %lu\n", local_size_B);
    return -1;
  }
  printf("gpu alloc success\n");
*/ 
 // Remote alloc
  alloc_params->rem_alloc_bytes = rem_size_B;
  #ifdef INFINIBAND
  alloc_params->kind = OCM_REMOTE_RDMA;
  #endif
  #ifdef EXTOLL
  alloc_params->kind = OCM_REMOTE_RMA;
  #endif
  remote_alloc = ocm_alloc(alloc_params);
  if (!remote_alloc) {
    printf("ocm_alloc failed on remote size %lu\n", rem_size_B);
    return -1;
  }

  printf("All allocations completed\n");  

  copy_params = (ocm_param_t)calloc(1,sizeof(struct ocm_params));
  copy_params->src_offset = 0;
  copy_params->dest_offset = 0;
  copy_params->bytes = local_size_B;
  copy_params->op_flag = 1;
 
  #ifdef CUDA 
  // GPU->host
  if(ocm_copy(local_alloc, gpu_alloc, copy_params)){
    printf("ocm_copy from GPU to host memory failed\n");
    return -1;    
  }
  
  // GPU->remote 
  if(ocm_copy(remote_alloc, gpu_alloc, copy_params)){
    printf("ocm_copy from GPU to remote memory failed\n");
    return -1;
  }
  #endif
  // Host->host
  if(ocm_copy(local_alloc2, local_alloc, copy_params)){
    printf("ocm_copy from host to host failed\n");
    return -1;
  }
  
  // Host->remote 
  if(ocm_copy(remote_alloc, local_alloc, copy_params)){
    printf("ocm_copy from host to remote failed\n");
    return -1;
  }
  #ifdef CUDA
  // Host->GPU
  if(ocm_copy(gpu_alloc, local_alloc, copy_params)){
    printf("ocm_copy from host to GPU failed\n");
    return -1;
  }
  #endif
  // remote->host
  if(ocm_copy(local_alloc,remote_alloc, copy_params)){
    printf("ocm_copy from remote to local failed\n");
    return -1;
  }
  #ifdef CUDA
  // remote->GPU
  if(ocm_copy(gpu_alloc, remote_alloc, copy_params)){
    printf("ocm_copy from remote RMDA to GPU failed\n");
    return -1;
  }
  #endif
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc != 5) {
usage:
    fprintf(stderr, "Usage: %s <which test> <test_suboption> <allocation size 1 in MB (alloc1)>"
        " <allocation size 2 in MB (alloc2)>\n "
	"\twhich test: 1=allocation; 2=copy-onesided; 3=copy-twosided\n" 
	"\tSuboptions: 1=allocate host memory; 2=allocate GPU memory; 3=allocate IB buffer (alloc1-local, alloc2-remote)\n"
	" \t\t4=allocate EXTOLL buffer (alloc1-local, alloc2-remote)\n", argv[0]);
    return -1;
  }

  //Convert the double values for MB input to bytes
  double local_size_MB = strtod(argv[3], 0);
  uint64_t local_size_B = (uint64_t)(local_size_MB*pow(2,20));

  //Convert the double values for MB input to bytes
  double rem_size_MB = strtod(argv[4], 0);
  uint64_t rem_size_B = (uint64_t)(rem_size_MB*pow(2,20));

  if(local_size_B > rem_size_B)
  {
    printf("Please use a larger remote buffer size than local size\n");
    return -1;
  }
  switch (atoi(argv[1])){
    //allocation
    case 1:
      printf("Testing OCM allocation with local buffer of size %4f MB and remote buffer of size %4f MB\n", local_size_MB, rem_size_MB);
      if(alloc_test(atoi(argv[2]),local_size_B, rem_size_B)){
        fprintf(stderr, "FAIL: allocation test\n");
	return -1;
      }
      else
	printf("pass: allocation test\n");
      break;     
    //copy-onesided
    case 2:
      if(copy_onesided_test(local_size_B,rem_size_B)){
	fprintf(stderr, "FAIL: allocation test\n");
	return -1;
      }
      else
	printf("pass: copy one-sided test\n");
      break;
    //copy-twosided
    case 3:
      if(copy_twosided_test(local_size_B, rem_size_B)){
	fprintf(stderr, "FAIL: copy twosided test\n");
	return -1;
      }
      else
	printf("pass: copy two-sided test\n");
      break;
    default:
      goto usage;
  }
  return 0;	
}
