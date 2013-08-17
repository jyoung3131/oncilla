/**
 * file: ocm_timer.h
 * author: Jeff Young, jyoung9@gatech.edu
 * desc: interface for timing of internal Oncilla functions
 */

#ifndef __OCM_TIMER_H__
#define __OCM_TIMER_H__

/* System includes */
#include <stdlib.h>  
#include <stdio.h>  
#include <stdint.h>
#include <limits.h>

/* Other project includes */

/* Project includes */
#ifdef CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#endif


struct oncilla_timer
{
  //Total amount of time for all transfers
  uint64_t tot_transfer_ns; 
  //time to transfer data to the host
  uint64_t host_transfer_ns;
  //time to transfer data from the host to the GPU
  uint64_t gpu_transfer_ns;

  uint64_t tot_transfer_B;
  uint64_t host_transfer_B;
  uint64_t gpu_transfer_B;

  //number of allocations used for timing tracking
  uint64_t num_allocs;
  //number of data transfers (one-sided or two-sided) used for timing tracking
  uint64_t num_transfers;
  
  //Total amount of time to create/teardown a connection
  uint64_t tot_setup_ns; 
  uint64_t tot_teardown_ns; 

  //*************Be very careful about placing any
  //general variables after the unions, as the struct's memory
  //layout can vary depending on whether EXTOLL or IB is used.
  //This caused some issues with random variables getting updated
  //due to incorrect pointer references.*****************

#ifdef CUDA
  cudaEvent_t cuStart, cuStop;
#endif

  union {
//#ifdef EXTOLL
    struct 
    {
      //allocation timers
      uint64_t conn_ns;
      uint64_t open_ns;
      uint64_t malloc_ns;
      uint64_t reg_ns;
      //deallocation timers
      uint64_t discon_ns;
      uint64_t close_ns;
      uint64_t dereg_ns;
    } rma;
//#endif
//#ifdef INFINIBAND
    struct 
    {
      //allocation timers
      uint64_t create_qp_ns;
      uint64_t malloc_ns;
      uint64_t reg_ns;    
      //deallocation timers
      uint64_t destroy_qp_ns;
      uint64_t dereg_ns;
      uint64_t tmp1;
      uint64_t tmp2;
    } rdma;
//#endif
  } alloc_tm;
  
  union {
#ifdef EXTOLL
    struct 
    {
      uint64_t put_get_ns;
    } rma;
#endif
#ifdef INFINIBAND
    struct 
    {
      uint64_t post_ns;
      uint64_t poll_ns; 
    } rdma;
#endif
  } data_tm; 
};

typedef struct oncilla_timer * ocm_timer_t;


//Helper function to reset the timer entries
static void reset_ocm_timer(ocm_timer_t* tm)
{
  memset(*tm, 0, sizeof(struct oncilla_timer));
}

static void init_ocm_timer(ocm_timer_t* tm)
{
  //Allocate the *max* size that the timer could take up to avoid
  //issues with data updates and pointer misdirection within the
  //struct

  int cudaSzB = 0;
#ifdef CUDA
  cudaSzB = 2*sizeof(cudaEvent_t);
#endif
  int timer_sz_B = (19*sizeof(uint64_t)) + cudaSzB;

  *tm = (ocm_timer_t)calloc(1, timer_sz_B);
  //Initialize the timer to 0
  reset_ocm_timer(tm);
  //Initialize cudaEvent timers
#ifdef CUDA
  cudaEventCreate(&((*tm)->cuStart));
  cudaEventCreate(&((*tm)->cuStop));
#endif
}

//Helper function to add values from one source time to a destination
//timer
static void accum_ocm_timer(ocm_timer_t* dest, const ocm_timer_t src)
{
#ifdef INFINIBAND
  (*dest)->alloc_tm.rdma.reg_ns += src->alloc_tm.rdma.reg_ns;
  (*dest)->alloc_tm.rdma.create_qp_ns += src->alloc_tm.rdma.create_qp_ns;
  (*dest)->alloc_tm.rdma.malloc_ns += src->alloc_tm.rdma.malloc_ns;
  (*dest)->alloc_tm.rdma.dereg_ns += src->alloc_tm.rdma.dereg_ns;
  (*dest)->alloc_tm.rdma.destroy_qp_ns += src->alloc_tm.rdma.destroy_qp_ns;

  (*dest)->data_tm.rdma.post_ns += src->data_tm.rdma.post_ns;
  (*dest)->data_tm.rdma.poll_ns += src->data_tm.rdma.poll_ns;
#endif
#ifdef EXTOLL
  (*dest)->alloc_tm.rma.conn_ns += src->alloc_tm.rma.conn_ns;
  (*dest)->alloc_tm.rma.open_ns += src->alloc_tm.rma.open_ns;
  (*dest)->alloc_tm.rma.malloc_ns += src->alloc_tm.rma.malloc_ns;
  (*dest)->alloc_tm.rma.reg_ns += src->alloc_tm.rma.reg_ns;
  (*dest)->alloc_tm.rma.discon_ns += src->alloc_tm.rma.discon_ns;
  (*dest)->alloc_tm.rma.close_ns += src->alloc_tm.rma.close_ns;
  (*dest)->alloc_tm.rma.dereg_ns += src->alloc_tm.rma.dereg_ns;
#endif

  (*dest)->tot_setup_ns += src->tot_setup_ns;
  (*dest)->tot_teardown_ns += src->tot_teardown_ns;

  (*dest)->host_transfer_ns += src->host_transfer_ns;
  (*dest)->gpu_transfer_ns += src->gpu_transfer_ns;
  (*dest)->tot_transfer_ns += src->tot_transfer_ns;
}

static void print_ocm_alloc_timer(ocm_timer_t tm)
{
  printf("\nAverage times for %lu allocations:"
      "\n-----------------\n",tm->num_allocs);

  if(tm->num_allocs == 0)
    return;

#ifdef INFINIBAND
  printf("malloc: %6f ns\n"
      "ibv_reg_mr: %6f ns\n"
      "rdma_create_qp: %6f ns\n",
      ((double)tm->alloc_tm.rdma.malloc_ns)/((double)tm->num_allocs),
      ((double)tm->alloc_tm.rdma.reg_ns)/((double)tm->num_allocs), ((double)tm->alloc_tm.rdma.create_qp_ns)/((double)tm->num_allocs));
#elif EXTOLL
  printf("malloc: %6f ns\n"
      "rma_register: %6f ns\n"
      "rma_open: %6f ns\n"
      "rma_connect: %6f ns\n",
      (double)(tm->alloc_tm.rma.malloc_ns/tm->num_allocs), (double)(tm->alloc_tm.rma.reg_ns/tm->num_allocs),
      (double)(tm->alloc_tm.rma.open_ns/tm->num_allocs), (double)(tm->alloc_tm.rma.conn_ns/tm->num_allocs));
#endif
   
  printf("Total time for connection: %6f ns\n\n", (double)(tm->tot_setup_ns/tm->num_allocs));
}

static void print_ocm_teardown_timer(ocm_timer_t tm)
{
  printf("\nAverage times for %lu teardowns:"
      "\n-----------------\n",tm->num_allocs);
  
  if(tm->num_allocs == 0)
    return;

#ifdef INFINIBAND
  printf("ibv_unreg_mr: %6f ns\n"
      "rdma_destroy_qp: %6f ns\n",
      (double)(tm->alloc_tm.rdma.dereg_ns/tm->num_allocs), (double)(tm->alloc_tm.rdma.destroy_qp_ns/tm->num_allocs));
#elif EXTOLL
  printf("rma_unregister: %6f ns\n"
      "rma_close: %6f ns\n"
      "rma_disconnect: %6f ns\n",
      (double)(tm->alloc_tm.rma.dereg_ns/tm->num_allocs), (double)(tm->alloc_tm.rma.close_ns/tm->num_allocs),
      (double)(tm->alloc_tm.rma.discon_ns/tm->num_allocs));
#endif

  printf("Total time for teardown: %6f ns\n\n", (double)(tm->tot_teardown_ns/tm->num_allocs));
}

static void print_ocm_transfer_timer(ocm_timer_t tm)
{

  printf("\nAverage times for %lu transfers:"
      "\n-----------------\n",tm->num_transfers);
  
  if(tm->num_transfers == 0)
    return;

#ifdef INFINIBAND
  printf("IB post: %lu ns, poll: %lu ns\n", tm->data_tm.rdma.post_ns, tm->data_tm.rdma.poll_ns);
#elif EXTOLL
  printf("EXTOLL put or get: %lu ns\n", tm->data_tm.rma.put_get_ns);
#endif

  printf("Time for host transfers: %6f ns\n"
      "Time for GPU transfers: %6f ns\n",
      (double)(tm->host_transfer_ns/tm->num_transfers), (double)(tm->gpu_transfer_ns/tm->num_transfers));

  printf("Total time for transfers: %6f ns\n", (double)(tm->tot_transfer_ns/tm->num_transfers));

}

static void print_ocm_timer(ocm_timer_t tm)
{
  print_ocm_alloc_timer(tm);
  
  print_ocm_teardown_timer(tm);
  
  print_ocm_transfer_timer(tm);

  printf("\n\n");
}

static void destroy_ocm_timer(ocm_timer_t tm)
{
  free(tm);
}

#endif //OCM_TIMER 
