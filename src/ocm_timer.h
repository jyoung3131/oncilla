/**
 * file: ocm_timer_t.h
 * author: Jeff Young, jyoung9@gatech.edu
 * desc: interface for timing of internal Oncilla functions
 */

#ifndef __OCM_TIMER_H__
#define __OCM_TIMER_H__

/* System includes */
#include <stdint.h>
#include <limits.h>

/* Other project includes */

/* Project includes */

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

    uint64_t transfer_ns;
};

typedef struct oncilla_timer * ocm_timer_t;

//Helper function to reset the timer entries
static void reset_timer(ocm_timer_t* tm)
{
  memset(tm, 0, sizeof(ocm_timer_t));
}

#endif 
