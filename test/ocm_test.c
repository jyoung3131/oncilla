#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <oncillamem.h>
#include <math.h>

#include <util/timer.h>
//Needed to explicitly close EXTOLL connections
#ifdef EXTOLL
#include <io/extoll.h>
#include "../src/extoll.h"
#endif

#ifdef CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#endif

void print_usage(const char* prog_name)
{
  fprintf(stderr, "Usage: %s <which test> <allocation size 1 in MB (alloc1)> <allocation size 2 in MB (alloc2)> "
      "<suboption1_allocation_type> <suboption2_test4_num_iter>\n"
      "\tWhich test: 1=allocation; 2=copy-onesided; 3=copy-twosided; 4=read/write BW\n"
      "\t\tSuboptions for test 1: 1=allocate host memory; 2=allocate GPU memory; \n"
      "\t\t\t\t3=allocate IB buffer (alloc1-local, alloc2-remote); 4=allocate EXTOLL buffer (alloc1-local, alloc2-remote)\n"
      "\t\tSuboptions for test 4: type of allocation (IB=0, EXTOLL=1); number iterations\n\n"
      "\tEx: Test 1 with IB memory: %s 1 10.0 10.0 3\n"
      "\tEx: Test 2 with 10 MB memory: %s 2 10.0 10.0\n"
      "\tEx: Test 3 with 10 MB memory: %s 3 10.0 10.0\n"
      "\tEx: Test 4 BW test for EXTOLL, 5 iterations: %s 4 1 5\n", prog_name, prog_name, prog_name, prog_name, prog_name);
}

static int alloc_test(int suboption, uint64_t local_size_B, uint64_t rem_size_B){

  TIMER_DECLARE1(ocm_alloc_timer);

  int num_allocs = 5;
  ocm_alloc_t a[num_allocs];
  void *buf;
  size_t buf_len, remote_len;
  ocm_alloc_param_t alloc_params;
  int i;

  ocm_timer_t tm;
  init_ocm_timer(&tm);

  //Timer that tracks allocations for multiple iterations
  ocm_timer_t tot_timer;
  init_ocm_timer(&tot_timer);

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  //Create a structure that is used to configure the allocation on each endpoint
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  //The allocation pointer points to the same structure as tm defined above
  alloc_params->tm = tm;

  alloc_params->local_alloc_bytes = local_size_B;
  switch (suboption){
    case 1:
      alloc_params->kind = OCM_LOCAL_HOST;
      printf("Testing allocation of local memory\n");
      break;
    case 2:
#ifdef CUDA
      alloc_params->kind = OCM_LOCAL_GPU;
      printf("Testing allocation of local GPU memory\n");
#else
      printf("No CUDA support - exiting\n");
      exit(-1);
#endif

      break;
    case 3:
      alloc_params->kind = OCM_REMOTE_RDMA;
      alloc_params->rem_alloc_bytes = rem_size_B;
      printf("Testing allocation of remote IB memory\n");
      break;
    case 4:
      alloc_params->kind = OCM_REMOTE_RMA;
      alloc_params->rem_alloc_bytes = rem_size_B;
      printf("Testing allocation of remote EXTOLL memory\n");
      break;
    default:
      print_usage("ocm_test");
  }

  uint64_t ocm_alloc_ns = 0;
  uint64_t ocm_teardown_ns = 0;
  uint64_t ocm_tmp_ns = 0;

  for(i = 0; i < num_allocs; i++)
  {

    TIMER_START(ocm_alloc_timer);
    a[i] = ocm_alloc(alloc_params);
    TIMER_END(ocm_alloc_timer, ocm_tmp_ns);
    TIMER_CLEAR(ocm_alloc_timer);
      
    printf("OCM allocation time is %lu ns\n\n", ocm_tmp_ns);
    ocm_alloc_ns += ocm_tmp_ns;

    if (!a[i]) {
      printf("ocm_alloc failed on remote size %lu\n", rem_size_B);
      return -1;
    }

    printf("Checking to see if ocm_localbuf works.\n");
    //Check to see if we can access a local buffer endpoint
    if (ocm_localbuf(a[i], &buf, &buf_len)) 
    {
      printf("ocm_localbuf failed\n");
      goto fail;
    }
    printf("local buffer size %lu @ %p\n", buf_len, buf);

    if (ocm_is_remote(a[i])) {
      if (!ocm_remote_sz(a[i], &remote_len)) {
        printf("alloc is remote; size = %lu\n", remote_len);
      } else {
        printf("alloc is local\n");
      }
    }

    TIMER_START(ocm_alloc_timer);
    if(ocm_free(a[i], tm))
    {
      printf("ocm_free failed\n");
      goto fail;
    }
    TIMER_END(ocm_alloc_timer, ocm_tmp_ns);
    TIMER_CLEAR(ocm_alloc_timer);
    
    printf("OCM teardown time is %lu ns\n\n", ocm_tmp_ns);
    ocm_teardown_ns += ocm_tmp_ns;
    
    //Add the timing values to a running counter timer
    tot_timer->num_allocs++;
    accum_ocm_timer(&tot_timer, tm);
  }
 
  print_ocm_timer(tot_timer);

  printf("Average Oncilla time for allocation: %6f ns, deallocation: %6f ns\n", (double)(ocm_alloc_ns/tot_timer->num_allocs), (double)(ocm_teardown_ns/tot_timer->num_allocs));

  destroy_ocm_timer(tm);
  free(alloc_params);

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
}

static int copy_onesided_test(uint64_t local_size_B, uint64_t rem_size_B){
  ocm_alloc_t a;
  ocm_alloc_param_t alloc_params;
  ocm_param_t copy_params;
  
  ocm_timer_t tm;
  init_ocm_timer(&tm);

  //Using a very large buffer will avoid any crashing behavior.
  //uint64_t alloc_size_B = pow(2,31)+1;
  uint64_t alloc_size_B = local_size_B+1;

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  //Create a structure that is used to configure the allocation on each endpoint
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  alloc_params->tm = tm;
  alloc_params->local_alloc_bytes = alloc_size_B;
  alloc_params->rem_alloc_bytes = alloc_size_B;
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
  //It's ok to use the same timer for allocation and transfers since
  //they update exclusive time values
  copy_params->tm = tm;


  //Use a one-sided copy since we are copying from a local-remote IB or EXTOLL
  //paired object

  printf("Reading for size %lu\n", local_size_B);
  if(ocm_copy_onesided(a, copy_params)){
    printf("ocm_copy_onesided (read) failed\n");
    goto fail;
  } 

  copy_params->op_flag = 1;

  printf("Writing for size %lu\n", local_size_B);
  if(ocm_copy_onesided(a, copy_params)){
    printf("ocm_copy_onesided (write) failed\n");
    goto fail;
  } 

  ocm_free(a, tm);

  print_ocm_timer(tm);

  free(tm);
  free(alloc_params);
  free(copy_params);

  if (0 > ocm_tini()) {
    printf("ocm_tini failed\n");
    return -1;
  }

  printf("OCM test completed successfully\n");
  return 0;

fail:
  ocm_free(a, tm);
  free(tm);
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
  
  ocm_timer_t tm;
  init_ocm_timer(&tm);

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

#ifdef CUDA
  //GPU allocation
  alloc_params->kind = OCM_LOCAL_GPU;
  gpu_alloc = ocm_alloc(alloc_params);
  if (!gpu_alloc) {
    printf("gpu ocm_alloc failed on gpu alloc size %lu\n", local_size_B);
    return -1;
  }
  printf("gpu alloc success\n");
#endif

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
  copy_params->tm = tm;
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

  //Print out timing information and reset the timer
  print_ocm_transfer_timer(tm);
  reset_ocm_timer(&tm);

  // GPU->remote 
  if(ocm_copy(remote_alloc, gpu_alloc, copy_params)){
    printf("ocm_copy from GPU to remote memory failed\n");
    return -1;
  }
  
  print_ocm_transfer_timer(tm);
  reset_ocm_timer(&tm);
#endif
  // Host->host
  if(ocm_copy(local_alloc2, local_alloc, copy_params)){
    printf("ocm_copy from host to host failed\n");
    return -1;
  }
  
  print_ocm_transfer_timer(tm);
  reset_ocm_timer(&tm);

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
  
  print_ocm_transfer_timer(tm);
  reset_ocm_timer(&tm);

#endif
  // remote->host
  if(ocm_copy(local_alloc,remote_alloc, copy_params)){
    printf("ocm_copy from remote to local failed\n");
    return -1;
  }
  
  print_ocm_transfer_timer(tm);
  reset_ocm_timer(&tm);
#ifdef CUDA
  // remote->GPU
  if(ocm_copy(gpu_alloc, remote_alloc, copy_params)){
    printf("ocm_copy from remote memory to GPU failed\n");
    return -1;
  }
  
  print_ocm_transfer_timer(tm);
  reset_ocm_timer(&tm);
#endif
  return 0;
}

static int read_write_bw_test(int num_iter, int alloc_type){
  ocm_alloc_t a;

#ifdef TIMING
  double ocm_tot_bw_ns = 0;
#endif     

  ocm_alloc_param_t alloc_params;
  ocm_param_t copy_params;
  uint64_t local_size_B = 64;
  uint64_t alloc_size_B = pow(2,31)+1;
  uint64_t max_rw_size_B = pow(2,30);
  int i;
  int counter=0;
  ocm_timer_t alloc_tm, tm;
  init_ocm_timer(&alloc_tm);
  init_ocm_timer(&tm);

  double conv_Gbps = 1000000000.0 / (double)(pow(2, 27));

  if (0 > ocm_init()) {
    printf("Cannot connect to OCM\n");
    return -1;
  }

  //Create a structure that is used to configure the allocation on each endpoint
  alloc_params = calloc(1, sizeof(struct ocm_alloc_params));
  alloc_params->local_alloc_bytes = alloc_size_B;
  alloc_params->rem_alloc_bytes = alloc_size_B;
  if(alloc_type == 0)
    alloc_params->kind = OCM_REMOTE_RDMA;
  else //alloc_type == 1
    alloc_params->kind = OCM_REMOTE_RMA;

  alloc_params->tm = alloc_tm;

  a = ocm_alloc(alloc_params);
  if (!a) {
    printf("ocm_alloc failed on remote size %lu\n", alloc_size_B);
    return -1;
  }

  copy_params = (ocm_param_t)calloc(1,sizeof(struct ocm_params));
  copy_params->tm = tm;
  copy_params->src_offset = 0;
  copy_params->dest_offset = 0;
  //set the operation to a read, initially
  copy_params->op_flag = 0;

  //---------------------------
  //Do the read bandwidth test
  //---------------------------
  while(local_size_B <= (max_rw_size_B)){
    //Use a one-sided copy since we are copying from a local-remote IB
    //paired object
    printf("Reading for size %lu\n", local_size_B);
    copy_params->bytes= local_size_B;
    ocm_tot_bw_ns = 0;  

    for (i=0; i<num_iter; i++){
      counter++;
      //printf("reading\n");
      if(ocm_copy_onesided(a, copy_params)){
        printf("ocm_copy_onesided (read) failed at size %lu\n",local_size_B);
        goto fail;
      }
      //Each iteration adds the time to tm
    }
    ocm_tot_bw_ns = ((double)(copy_params->tm->tot_transfer_ns)) / ((double)num_iter);
    printf("Read for size %lu bytes took %6f ns and had BW of %4f Gb/s\n", local_size_B, ocm_tot_bw_ns, (((double)local_size_B)/(ocm_tot_bw_ns))*conv_Gbps);

    local_size_B*=2;
    //Reset the timer each time
    reset_ocm_timer(&tm);
  }


  //---------------------------
  //Do the write bandwidth test
  //---------------------------
  local_size_B=64;
  copy_params->op_flag = 1;
  int c=0;

  while(local_size_B <= (max_rw_size_B))
  {
    copy_params->bytes= local_size_B;
    printf("Writing for size %lu\n", local_size_B);
    ocm_tot_bw_ns = 0;  

    for (i=0; i<num_iter; i++){
      counter++;
      //printf("writing\n");
      if(ocm_copy_onesided(a, copy_params))
      {
        printf("ocm_copy_onesided (write) failed at size %lu count: %d\n", local_size_B, c);
        goto fail;
      }
      c++;
    }
    ocm_tot_bw_ns = (double)tm->tot_transfer_ns / (double)num_iter;
    printf("Read for size %lu bytes took %4f ns and had BW of %4f Gb/s\n", local_size_B, ocm_tot_bw_ns, (((double)local_size_B)/(ocm_tot_bw_ns))*conv_Gbps);
    
    local_size_B*=2;
    reset_ocm_timer(&tm);
  }

  ocm_free(a, alloc_tm);
  //Free allocation and configuration parameters
  //destroy_ocm_timer(tm);
  destroy_ocm_timer(alloc_tm);
  free(alloc_params);
  free(copy_params);

  if (0 > ocm_tini()) {
    printf("ocm_tini failed\n");
    return -1;
  }

  printf("OCM test completed successfully\n");
  return 0;

fail:

  ocm_free(a, tm);
  destroy_ocm_timer(tm);
  destroy_ocm_timer(alloc_tm);
  free(alloc_params);
  free(copy_params);
  if (0 > ocm_tini())
    printf("ocm_tini failed\n");
  return -1;
}


int main(int argc, char *argv[])
{
  double local_size_MB;
  uint64_t local_size_B;
  double rem_size_MB;
  uint64_t rem_size_B;

  //Test number to run 1-4
  int test_num;
  //allocation type for tests 1 and 4
  int alloc_type; 
  //number of iterations for bandwidth test, test 4
  int num_iter=1;

  //Check to see if the test type was specified
  if (argc < 2)
  {
    print_usage(argv[0]); 
    return -1;
  }

  //Check the test number to determine how to proceed
  test_num = atoi(argv[1]);
  printf("Test number %d selected\n", test_num);

  //All tests except the bandwidth test specify a size
  if(test_num != 4)
  {
    if((test_num == 1 && argc != 5) || ((test_num == 2 || test_num == 3) && argc != 4)) 
    {
      print_usage(argv[0]); 
      return -1;
    }
    //Convert the double values for MB input to bytes
    local_size_MB = strtod(argv[2], 0);
    local_size_B = (uint64_t)(local_size_MB*pow(2,20));

    //Convert the double values for MB input to bytes
    rem_size_MB = strtod(argv[3], 0);
    rem_size_B = (uint64_t)(rem_size_MB*pow(2,20));

    if(local_size_B > rem_size_B)
    {
      printf("Please use a larger remote buffer size than local size\n");
      return -1;
    }
  }

  switch (test_num){
    //allocation
    case 1:
      alloc_type = atoi(argv[4]);
      printf("Testing IB allocation with local buffer of size %4f MB and remote buffer of size %4f MB\n", local_size_MB, rem_size_MB);
      if(alloc_test(alloc_type,local_size_B, rem_size_B)){
        fprintf(stderr, "FAIL: allocation test\n");
        return -1;
      }
      else
        printf("pass: allocation test\n");
      break;
      //copy-onesided tests all allocation types available
    case 2:
      if(copy_onesided_test(local_size_B,rem_size_B)){
        fprintf(stderr, "FAIL: allocation test\n");
        return -1;
      }
      else
        printf("pass: copy one-sided test\n");
      break;
      //copy-twosided tests all allocation types available
    case 3:
      if(copy_twosided_test(local_size_B, rem_size_B)){
        fprintf(stderr, "FAIL: copy twosided test\n");
        return -1;
      }
      else
        printf("pass: copy two-sided test\n");
      break;
    case 4:
      if(argc != 4)
      {
        print_usage(argv[0]);
        return -1;
      }

      alloc_type = atoi(argv[2]);
      num_iter=atoi(argv[3]);
      printf("Calling R/W bandwidth test with %d iterations for each size and allocation type %d\n", num_iter, alloc_type);

      if(read_write_bw_test(num_iter, alloc_type)){
        fprintf(stderr, "FAIL: read/write bw test\n");
        return -1;
      }
      else
        printf("pass: read/write bw test\n");
      break;
    default:
      print_usage(argv[0]);
  }
  return 0;

  return -1;
}
