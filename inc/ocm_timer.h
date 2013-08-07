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
      //allocation timers
      uint64_t conn_ns;
      uint64_t open_ns;
      uint64_t malloc_ns;
      uint64_t reg_ns;
      uint64_t tot_conn_ns;
      //deallocation timers
      uint64_t discon_ns;
      uint64_t close_ns;
      uint64_t unreg_ns;
      uint64_t tot_discon_ns;
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

#ifdef CUDA
  cudaEvent_t cuStart, cuStop;
#endif
  
  //Total amount of time for all transfers
  uint64_t tot_transfer_ns; 
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
  (*dest)->alloc_tm.rdma.ib_mem_reg_ns += src->alloc_tm.rdma.ib_mem_reg_ns;
  (*dest)->alloc_tm.rdma.ib_create_qp_ns += src->alloc_tm.rdma.ib_create_qp_ns;
  (*dest)->alloc_tm.rdma.ib_total_conn_ns += src->alloc_tm.rdma.ib_total_conn_ns;
  (*dest)->alloc_tm.rdma.ib_mem_dereg_ns += src->alloc_tm.rdma.ib_mem_dereg_ns;
  (*dest)->alloc_tm.rdma.ib_destroy_qp_ns += src->alloc_tm.rdma.ib_destroy_qp_ns;
  (*dest)->alloc_tm.rdma.ib_total_disconnect_ns += src->alloc_tm.rdma.ib_total_disconnect_ns;

  (*dest)->data_tm.rdma.ib_post_ns += src->data_tm.rdma.ib_post_ns;
  (*dest)->data_tm.rdma.ib_poll_ns += src->data_tm.rdma.ib_poll_ns;
#endif
#ifdef EXTOLL
  (*dest)->alloc_tm.rma.conn_ns += src->alloc_tm.rma.conn_ns;
  (*dest)->alloc_tm.rma.open_ns += src->alloc_tm.rma.open_ns;
  (*dest)->alloc_tm.rma.malloc_ns += src->alloc_tm.rma.malloc_ns;
  (*dest)->alloc_tm.rma.reg_ns += src->alloc_tm.rma.reg_ns;
  (*dest)->alloc_tm.rma.tot_conn_ns += src->alloc_tm.rma.tot_conn_ns;
  (*dest)->alloc_tm.rma.discon_ns += src->alloc_tm.rma.discon_ns;
  (*dest)->alloc_tm.rma.close_ns += src->alloc_tm.rma.close_ns;
  (*dest)->alloc_tm.rma.unreg_ns += src->alloc_tm.rma.unreg_ns;
  (*dest)->alloc_tm.rma.tot_discon_ns += src->alloc_tm.rma.tot_discon_ns;
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
      "[CONNECT] Total time for connection: %6f ns\n",
      (double)(tm->alloc_tm.rdma.ib_mem_reg_ns/tm->num_allocs), (double)(tm->alloc_tm.rdma.ib_create_qp_ns/tm->num_allocs), (double)(tm->alloc_tm.rdma.ib_total_conn_ns/tm->num_allocs));
#endif

}

static void destroy_ocm_timer(ocm_timer_t tm)
{
  free(tm);
}

#endif 
