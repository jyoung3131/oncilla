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

  union {
#ifdef EXTOLL
    struct 
    {
    } rma;
#endif
#ifdef INFINIBAND
    struct 
    {
      //allocation timers
      uint64_t ib_mem_reg_ns;    
      uint64_t ib_create_qp_ns;
      uint64_t ib_total_conn_ns;
      //deallocation timers
      uint64_t ib_total_disconnect_ns;
      uint64_t ib_destroy_qp_ns;
      uint64_t ib_mem_dereg_ns; 
    } rdma;
#endif
  } alloc;

  union {
#ifdef EXTOLL
    struct 
    {
    } rma;
#endif
#ifdef INFINIBAND
    struct 
    {
      uint64_t ib_post_ns;
      uint64_t ib_poll_ns; 
    } rdma;
#endif
  } data;

#ifdef CUDA
  cudaEvent_t cuStart, cuStop;
#endif
  //time to transfer data to the host
  uint64_t host_transfer_ns;
  //time to transfer data from the host to the GPU
  uint64_t gpu_transfer_ns;

  //number of allocations used for timing tracking
  uint64_t num_allocs;
  //number of data transfers (one-sided or two-sided) used for timing tracking
  uint64_t num_transfers;
};

typedef struct oncilla_timer * ocm_timer_t;


//Helper function to reset the timer entries
static void reset_ocm_timer(ocm_timer_t* tm)
{
  memset(*tm, 0, sizeof(struct oncilla_timer));
}

static void init_ocm_timer(ocm_timer_t* tm)
{
  *tm = (ocm_timer_t)calloc(1, sizeof(struct oncilla_timer));
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
    (*dest)->alloc.rdma.ib_mem_reg_ns += src->alloc.rdma.ib_mem_reg_ns;
    (*dest)->alloc.rdma.ib_create_qp_ns += src->alloc.rdma.ib_create_qp_ns;
    (*dest)->alloc.rdma.ib_total_conn_ns += src->alloc.rdma.ib_total_conn_ns;
    (*dest)->alloc.rdma.ib_mem_dereg_ns += src->alloc.rdma.ib_mem_dereg_ns;
    (*dest)->alloc.rdma.ib_destroy_qp_ns += src->alloc.rdma.ib_destroy_qp_ns;
    (*dest)->alloc.rdma.ib_total_disconnect_ns += src->alloc.rdma.ib_total_disconnect_ns;
    
    (*dest)->data.rdma.ib_post_ns += src->data.rdma.ib_post_ns;
    (*dest)->data.rdma.ib_poll_ns += src->data.rdma.ib_poll_ns;
  #endif
  #ifdef EXTOLL

  #endif

  (*dest)->host_transfer_ns += src->host_transfer_ns;
  (*dest)->gpu_transfer_ns += src->gpu_transfer_ns;
}

static void print_ocm_timer(ocm_timer_t tm)
{
  printf("Statistics for Oncilla timer:\n\t"
    "Allocations: %lu\n\t"
    "Transfers: %lu\n", tm->num_allocs, tm->num_transfers);

  #ifdef INFINIBAND
  printf("[CONNECT] Time for ibv_reg_mr: %6f ns\n"
         "[CONNECT] Time for rdma_create_qp: %6f ns\n"
         "[CONNECT] Time for total server connection: %6f ns\n",
          (double)(tm->alloc.rdma.ib_mem_reg_ns/tm->num_allocs), (double)(tm->alloc.rdma.ib_create_qp_ns/tm->num_allocs), (double)(tm->alloc.rdma.ib_total_conn_ns/tm->num_allocs));
  #endif

}

static void destroy_ocm_timer(ocm_timer_t tm)
{
  free(tm);
}

#endif 
