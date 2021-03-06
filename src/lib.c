/**
 * file: lib.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: library apps link with; libocm.so and oncillamem.h
 */

/* System includes */
#include <stdio.h>
#include <pthread.h>
#include <string.h>

/* Other project includes */

/* Project includes */
#include <oncillamem.h>
#include <pmsg.h>
#include <msg.h>
#include <debug.h>
#include <alloc.h>

/* Directory includes */
#ifdef CUDA
#include <cuda.h>
#include <cuda_runtime.h> 

#endif

#ifdef EXTOLL
#include "extoll.h"
#endif

/* Globals */

/* Internal definitions */

struct lib_alloc {
  struct list_head link;
  enum ocm_kind kind;
  //A unique allocation ID per node to allow sending ocm_free messages to
  //remote nodes
  uint64_t rem_alloc_id;
  /* TODO Later, when allocations are composed of partitioned distributed
   * allocations, this will no longer be a single union, but an array of them,
   * to accomodate the heterogeneity in allocations.
   */
  union {
#ifdef INFINIBAND
    struct {
      ib_t ib;
      int remote_rank;
      size_t remote_bytes;
      size_t local_bytes;
      void *local_ptr;
    } rdma;
#endif
#ifdef EXTOLL
    struct {
      extoll_t ex;
      int remote_rank;
      size_t remote_bytes;
      size_t local_bytes;
      void *local_ptr;
    } rma;
#endif
    //Local allocation
    struct {
      size_t bytes;
      void *ptr;
    } local;
    //GPU allocation
#ifdef CUDA
    struct {
      size_t bytes;
      void *cuda_ptr;
    } gpu;
#endif
  } u;
};



/* Internal state */

static LIST_HEAD(allocs); /* list of lib_alloc */
static pthread_mutex_t allocs_lock = PTHREAD_MUTEX_INITIALIZER;


#define for_each_alloc(alloc, allocs) \
  list_for_each_entry(alloc, &allocs, link)
#define lock_allocs()   pthread_mutex_lock(&allocs_lock)
#define unlock_allocs() pthread_mutex_unlock(&allocs_lock)

/* Private functions */

/* Global functions */

  int
ocm_init(void)
{
  struct message msg;
  int tries = 10; /* to open daemon mailbox */
  bool opened = false, attached = false;
  int ret = -1;

  /* open resources */
  if (pmsg_init(sizeof(struct message)))
    goto out;
  if (pmsg_open(getpid()))
    goto out;
  opened = true;
  while ((usleep(10000), tries--) > 0)
    if (0 == pmsg_attach(PMSG_DAEMON_PID))
      break;
  if (tries <= 0)
    goto out;
  attached = true;

#ifdef INFINIBAND
  if (ib_init()) {
    goto out;
  }
#endif

  /* tell daemon who we are, wait for confirmation msg */
  memset(&msg, 0, sizeof(msg));
  msg.type = MSG_CONNECT;
  msg.pid = getpid();
  if (pmsg_send(PMSG_DAEMON_PID, &msg))
    goto out;
  if (pmsg_recv(&msg, true))
    goto out;
  if (msg.type != MSG_CONNECT_CONFIRM)
    goto out;
  ret = 0;

out:
  printd("attach to daemon: %s\n", (ret ? "fail" : "success"));
  if (ret) {
    if (attached)
      pmsg_detach(PMSG_DAEMON_PID);
    if (opened)
      pmsg_close();
  }
  return ret;
}

  int
ocm_tini(void)
{
  struct message msg;
  int ret = -1;
  msg.type = MSG_DISCONNECT;
  msg.pid = getpid();
  if (pmsg_send(PMSG_DAEMON_PID, &msg))
    goto out;
  if (pmsg_detach(PMSG_DAEMON_PID))
    goto out;
  if (pmsg_close())
    goto out;
  ret = 0;

out:
  printd("detach from daemon: %s\n", (ret ? "fail" : "success"));
  return ret;
}

//Public access function to return the type of an allocation
enum ocm_kind ocm_alloc_kind(ocm_alloc_t alloc)
{
  return alloc->kind;
}

//Allocation function, ocm_alloc
  ocm_alloc_t
ocm_alloc(ocm_alloc_param_t alloc_param)
{
  struct message msg;
  struct lib_alloc *alloc;
  int ret = -1;

  alloc = calloc(1, sizeof(*alloc));
  if (!alloc)
    goto out;

  msg.type        = MSG_REQ_ALLOC;
  msg.status      = MSG_REQUEST;
  msg.pid         = getpid();
  //Specify the allocation size of the remote buffer; in
  //the local case the local_alloc_bytes field is used since
  //we will end up making a local allocation
  msg.u.req.bytes = alloc_param->rem_alloc_bytes;

  if (alloc_param->kind == OCM_LOCAL_HOST)
  {
    msg.u.req.type = ALLOC_MEM_HOST;
    msg.u.req.bytes = alloc_param->local_alloc_bytes;
  }
  else if(alloc_param->kind == OCM_LOCAL_GPU)
  {
    msg.u.req.type = ALLOC_MEM_GPU;
    msg.u.req.bytes = alloc_param->local_alloc_bytes;
  }
  else if (alloc_param->kind == OCM_REMOTE_RDMA)
    msg.u.req.type = ALLOC_MEM_RDMA;
  else if (alloc_param->kind == OCM_REMOTE_RMA)
    msg.u.req.type = ALLOC_MEM_RMA;
  else
  {
    printf("No allocation type specified\n");
    goto out;
  }

  printd("sending req_alloc to daemon\n");
  if (pmsg_send(PMSG_DAEMON_PID, &msg))
    goto out;

  printd("waiting for reply from daemon\n");
  if (pmsg_recv(&msg, true))
    goto out;
  BUG(msg.type != MSG_RELEASE_APP);

  if (msg.u.alloc.type == ALLOC_MEM_HOST) {
    printd("ALLOC_MEM_HOST %lu bytes\n", msg.u.alloc.bytes);
    alloc->kind             = OCM_LOCAL_HOST;
    alloc->u.local.bytes    = msg.u.alloc.bytes;
    alloc->u.local.ptr      = malloc(msg.u.alloc.bytes);
    if (!alloc->u.local.ptr)
      goto out;
  }
#ifdef CUDA
  else if (msg.u.alloc.type == ALLOC_MEM_GPU) {
    printd("ALLOC_MEM_GPU %lu bytes\n", msg.u.alloc.bytes);

    INIT_LIST_HEAD(&alloc->link);
    alloc->kind             = OCM_LOCAL_GPU;
    alloc->u.gpu.bytes    = msg.u.alloc.bytes;

    alloc->u.gpu.cuda_ptr = NULL;

    if(cudaMalloc((void**)&(alloc->u.gpu.cuda_ptr), msg.u.alloc.bytes)==cudaErrorMemoryAllocation)
    {
      goto out;
    }

    printd("adding new alloc to list\n");
    lock_allocs();
    list_add(&alloc->link, &allocs);
    unlock_allocs();
  }

#endif
#ifdef INFINIBAND
  else if (msg.u.alloc.type == ALLOC_MEM_RDMA) {
    printd("ALLOC_MEM_RDMA %lu bytes\n", msg.u.alloc.bytes);
    struct ib_params p;
    p.addr      = strdup(msg.u.alloc.u.rdma.ib_ip);
    p.port      = msg.u.alloc.u.rdma.port;
    p.buf_len   = alloc_param->local_alloc_bytes;
    p.buf       = malloc(p.buf_len);
    if (!p.buf)
      goto out;

    printd("RDMA: local buf %lu bytes <-->"
        " server %s:%d (rank%d) buf %lu bytes\n",
        p.buf_len, p.addr, p.port,
        msg.u.alloc.remote_rank, msg.u.alloc.bytes);

    alloc->u.rdma.ib = ib_new(&p);
    if (!alloc->u.rdma.ib)
      goto out;

    INIT_LIST_HEAD(&alloc->link);
    alloc->kind                 = OCM_REMOTE_RDMA;
    alloc->u.rdma.remote_rank   = msg.u.alloc.remote_rank;
    alloc->u.rdma.remote_bytes  = msg.u.alloc.bytes;
    alloc->u.rdma.local_bytes   = p.buf_len;
    alloc->u.rdma.local_ptr     = p.buf;
    alloc->rem_alloc_id         = msg.u.alloc.rem_alloc_id;

    if (ib_connect(alloc->u.rdma.ib, false))
      goto out;

    printd("adding new alloc to list\n");
    lock_allocs();
    list_add(&alloc->link, &allocs);
    unlock_allocs();

    free(p.addr);
    p.addr = NULL;
  }
#endif 

#ifdef EXTOLL
  else if (msg.u.alloc.type == ALLOC_MEM_RMA) {
    printd("ALLOC_MEM_RMA %lu bytes\n", msg.u.alloc.bytes);
    struct extoll_params p;
    p.buf_len   = alloc_param->local_alloc_bytes;
    p.dest_node = msg.u.alloc.u.rma.node_id;
    p.dest_vpid = msg.u.alloc.u.rma.vpid;
    p.dest_nla =  msg.u. alloc.u.rma.dest_nla;

    //The client will allocate the buffer p.buf

    printd("RDMA: local buf %lu bytes <-->"
        " (remote rank%d) buf %lu bytes\n",
        p.buf_len, 
        msg.u.alloc.remote_rank, msg.u.alloc.bytes);
    alloc->u.rma.ex = extoll_new(&p);
    if (!alloc->u.rma.ex)
      goto out;

    INIT_LIST_HEAD(&alloc->link);
    alloc->kind                 = OCM_REMOTE_RMA;
    alloc->u.rma.remote_rank   = msg.u.alloc.remote_rank;
    alloc->u.rma.remote_bytes  = msg.u.alloc.bytes;
    alloc->u.rma.local_bytes   = p.buf_len;
    alloc->rem_alloc_id        = msg.u.alloc.rem_alloc_id;

    if (extoll_connect(alloc->u.rma.ex, false))
      goto out;

    //Once the connection is complete the buffer is allocated
    alloc->u.rma.local_ptr = alloc->u.rma.ex->rma_conn.buf;

    printd("adding new alloc to list\n");
    lock_allocs();
    list_add(&alloc->link, &allocs);
    unlock_allocs();
  }
#endif
  else {
    BUG(1);
  }

  ret = 0;

out:
  if (ret) {
    if (alloc)
      free(alloc);
    alloc = NULL;
  }
  return alloc;
}

  int
ocm_free(ocm_alloc_t a)
{
  //We must transfer essential data (remote_rank, rem_alloc_id)
  //to the message's 'alloc_ation' struct, alloc from the local
  //lib_alloc structure
  struct message msg;

  msg.type        = MSG_REQ_FREE;
  msg.status      = MSG_REQUEST;
  msg.pid         = getpid();
  msg.u.alloc.rem_alloc_id  = a->rem_alloc_id;

  if (!a) return -1;
  if (a->kind == OCM_LOCAL_HOST) {
    free(a->u.local.ptr);
  }
#ifdef CUDA
  else if (a->kind == OCM_LOCAL_GPU) {
    cudaFree(a->u.gpu.cuda_ptr);
  }
#endif
#ifdef INFINIBAND
  else if (a->kind == OCM_REMOTE_RDMA)
  {

    msg.u.alloc.type = ALLOC_MEM_RDMA;
    msg.u.alloc.remote_rank = a->u.rdma.remote_rank;
    printd("sending req_free to daemon\n");
    if (pmsg_send(PMSG_DAEMON_PID, &msg))
      return -1;

    printd("waiting for reply from daemon\n");
    if (pmsg_recv(&msg, true))
      return -1;

    BUG(msg.type != MSG_RELEASE_APP);

    //release the local IB connection 
    if (ib_disconnect(a->u.rdma.ib, false/*is client*/))
      return -1;

    //Free the EXTOLL structure
    if(ib_free(a->u.rdma.ib))
      return -1;
  }
#endif
#ifdef EXTOLL
  else if (a->kind == OCM_REMOTE_RMA)
  {
    msg.u.alloc.type = ALLOC_MEM_RMA;
    msg.u.alloc.remote_rank = a->u.rma.remote_rank;
    printd("sending req_free to daemon\n");
    if (pmsg_send(PMSG_DAEMON_PID, &msg))
      return -1;

    printd("waiting for reply from daemon\n");
    if (pmsg_recv(&msg, true))
      return -1;
    BUG(msg.type != MSG_RELEASE_APP);

    //release the local EXTOLL connection 
    if (extoll_disconnect(a->u.rma.ex, false/*is client*/))
      return -1;

    //Free the EXTOLL structure
    if(extoll_free(a->u.rma.ex))
      return -1;

  }
#endif
  else
  {
    BUG(1);
  }
  return 0;
}

  int
ocm_localbuf(ocm_alloc_t a, void **buf, size_t *len)
{
  if (!a) return -1;
  if (a->kind == OCM_LOCAL_HOST) {
    *buf = a->u.local.ptr;
    *len = a->u.local.bytes;
  }
#ifdef CUDA
  else if (a->kind == OCM_LOCAL_GPU) {
    *buf = a->u.gpu.cuda_ptr;
    *len = a->u.gpu.bytes;
  }
#endif
#ifdef INFINIBAND
  else if (a->kind == OCM_REMOTE_RDMA) {
    *buf = a->u.rdma.local_ptr;
    *len = a->u.rdma.local_bytes;
  }
#endif
#ifdef EXTOLL
  else if (a->kind == OCM_REMOTE_RMA) {
    *buf = a->u.rma.local_ptr;
    *len = a->u.rma.local_bytes;
  } 
#endif
  else {
    BUG(1);
  }
  return 0;
}

  bool
ocm_is_remote(ocm_alloc_t a)
{
  bool is_remote = true;

  if((a->kind == OCM_LOCAL_HOST) || (a->kind != OCM_LOCAL_GPU))
    is_remote = false;

  return is_remote;
}

  int
ocm_remote_sz(ocm_alloc_t a, size_t *len)
{
  if (!a) return -1;
  if ((a->kind == OCM_LOCAL_HOST) || (a->kind == OCM_LOCAL_GPU))
  {
    return -1; /* there exists no remote buffer */
  }
#ifdef INFINIBAND
  else if (a->kind == OCM_REMOTE_RDMA) {
    *len = a->u.rdma.remote_bytes;
  } 
#endif
#ifdef EXTOLL
  else if (a->kind == OCM_REMOTE_RMA) {
    *len = a->u.rma.remote_bytes;
  }
#endif
  else {
    BUG(1);
  }
  return 0;
}

int ocm_copy_out(void *dest, ocm_alloc_t src)
{
  return -1;
}

int ocm_copy_in(ocm_alloc_t dest, void *src)
{
  return -1;
}

  int
ocm_copy(ocm_alloc_t dest, ocm_alloc_t src, ocm_param_t cp_param)
{

  printd("Number of bytes in ocm_copy is %lu \n", cp_param->bytes);

#ifdef CUDA
  cudaError_t cudaErr;
#endif
  //For read operations just reverse the order of the parameters
  if (!cp_param->op_flag)
  {
    cp_param->op_flag = 1;
    return ocm_copy(src, dest, cp_param);
  }

  //Local host to other OCM allocation
  if (src->kind == OCM_LOCAL_HOST)
  {
    //Do a standard memcpy to a local host
    if(dest->kind == OCM_LOCAL_HOST)
    {
      memcpy(dest->u.local.ptr+cp_param->dest_offset, src->u.local.ptr+cp_param->src_offset, cp_param->bytes);
    }
#ifdef INFINIBAND
    else if(dest->kind == OCM_REMOTE_RDMA)
    {
      //Do a memcpy to the local buffer and then write to the remote
      //IB buffer
      memcpy(dest->u.rdma.local_ptr+cp_param->dest_offset, src->u.local.ptr+cp_param->src_offset, cp_param->bytes);
      if(ib_write(dest->u.rdma.ib, cp_param->src_offset_2, cp_param->dest_offset_2, cp_param->bytes)||ib_poll(dest->u.rdma.ib))
        return -1;
    }
#endif
#ifdef EXTOLL
    else if(dest->kind == OCM_REMOTE_RMA)
    {
      //Do a memcpy to the local buffer and then write to the remote
      //EXTOLL buffer
      memcpy(dest->u.rma.local_ptr+cp_param->dest_offset, src->u.local.ptr+cp_param->src_offset, cp_param->bytes);

      if(extoll_write(dest->u.rma.ex, cp_param->src_offset_2, cp_param->dest_offset_2, cp_param->bytes))
      {
        printf("extoll_write failed in ocm_copy\n");
        return -1;
      }
    }
#endif
#ifdef CUDA
    else if(dest->kind == OCM_LOCAL_GPU)
    {
      cudaErr = cudaMemcpy(dest->u.gpu.cuda_ptr+cp_param->dest_offset, src->u.local.ptr+cp_param->src_offset, cp_param->bytes, cudaMemcpyHostToDevice);
      if(cudaErr)                                                     
      {                                                             
        printf("cudaMemcpy failed with error %d \n", cudaErr);  
        return -1;                                              
      }
    }
#endif
    else
    {
      BUG(1);
    }
  }
#ifdef INFINIBAND
  else if (src->kind == OCM_REMOTE_RDMA)
  {
    //Do a read from the remote IB buffer and then memcpy to the local buffer
    if(dest->kind == OCM_LOCAL_HOST)
    {
      //Remember to call both ib_read and ib_poll in order to correctly measure the time taken for the transfer
      if(ib_read(src->u.rdma.ib, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes)||ib_poll(src->u.rdma.ib))
        return -1;
      memcpy(dest->u.local.ptr+cp_param->dest_offset,src->u.rdma.local_ptr+cp_param->src_offset, cp_param->bytes);

    }
#ifdef CUDA
    else if(dest->kind == OCM_LOCAL_GPU)
    {
      if(ib_read(src->u.rdma.ib, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes)||ib_poll(src->u.rdma.ib))
        return -1;

      cudaErr = cudaMemcpy(dest->u.gpu.cuda_ptr+cp_param->dest_offset_2,src->u.rdma.local_ptr+cp_param->src_offset_2, cp_param->bytes, cudaMemcpyHostToDevice);
      if(cudaErr)
      {
        printf("cudaMemcpy failed with error %d \n", cudaErr);
        return -1;
      }
    }
#endif
    else
    {
      BUG(1);
    }
  }
#endif
#ifdef EXTOLL
  else if (src->kind == OCM_REMOTE_RMA)
  {
    //Do a read from the remote IB buffer and then memcpy to the local buffer
    if(dest->kind == OCM_LOCAL_HOST)
    {
      if(extoll_read(src->u.rma.ex, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes))
      {
        printf("extoll_read failed in ocm_copy\n");
        return -1;
      }

      memcpy(dest->u.local.ptr+cp_param->dest_offset,src->u.rma.local_ptr+cp_param->src_offset, cp_param->bytes);

    }
#ifdef CUDA
    else if(dest->kind == OCM_LOCAL_GPU)
    {
      if(extoll_read(src->u.rma.ex, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes))
      {
        printf("extoll_read failed in ocm_copy\n");
        return -1;
      }
      cudaErr = cudaMemcpy(dest->u.gpu.cuda_ptr+cp_param->dest_offset_2,src->u.rma.local_ptr+cp_param->src_offset_2, cp_param->bytes, cudaMemcpyHostToDevice);
      if(cudaErr)
      {
        printf("cudaMemcpy failed with error %d \n", cudaErr);
        return -1;
      }
    }
#endif
    else
    {
      BUG(1);
    }
  }
#endif
#ifdef CUDA
  else if (src->kind == OCM_LOCAL_GPU)
  {
    //Do a cudaMemcpy from GPU memory to the local host memory
    if(dest->kind == OCM_LOCAL_HOST)
    {
      cudaMemcpy(dest->u.local.ptr+cp_param->dest_offset, src->u.gpu.cuda_ptr+cp_param->src_offset, cp_param->bytes, cudaMemcpyDeviceToHost);
    }
#ifdef INFINIBAND
    else if(dest->kind == OCM_REMOTE_RDMA)
    {
      cudaMemcpy(dest->u.rdma.local_ptr+cp_param->dest_offset, src->u.gpu.cuda_ptr+cp_param->src_offset, cp_param->bytes, cudaMemcpyDeviceToHost);
      if(ib_write(dest->u.rdma.ib, cp_param->src_offset_2, cp_param->dest_offset_2, cp_param->bytes)||ib_poll(dest->u.rdma.ib))
        return -1;

    }
#endif
#ifdef EXTOLL
    else if(dest->kind == OCM_REMOTE_RMA)
    {
      cudaMemcpy(dest->u.rma.local_ptr+cp_param->src_offset_2, src->u.gpu.cuda_ptr+cp_param->dest_offset_2, cp_param->bytes, cudaMemcpyDeviceToHost);
      extoll_write(dest->u.rma.ex, cp_param->src_offset_2, cp_param->dest_offset_2, cp_param->bytes);
    }
#endif
  }
#endif
  else
  {
    BUG(1);
  }
  return 0;
}



  int
ocm_copy_onesided(ocm_alloc_t src, ocm_param_t cp_param)
{
  if((src->kind == OCM_LOCAL_HOST) || (src->kind == OCM_LOCAL_GPU))
  {
    printf("Error - one-sided copy needs a paired connection, such as IB or EXTOLL\n");
    return -1;
  }

#ifdef INFINIBAND
  if(cp_param->bytes > src->u.rdma.local_bytes)
    return -1;

  if (cp_param->op_flag)
  {

    if(ib_write(src->u.rdma.ib, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes)||ib_poll(src->u.rdma.ib)) 
    {
      printf("write failed\n");
      return -1;
    }
  }
  else
  {
    if(ib_read(src->u.rdma.ib, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes)||ib_poll(src->u.rdma.ib)) 
    {
      printf("read failed\n");
      return -1;
    }
  }
#endif
#ifdef EXTOLL
  if(cp_param->bytes > src->u.rma.local_bytes)
    return -1;

  if (cp_param->op_flag)
  {

    if(extoll_write(src->u.rma.ex, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes))
    {
      printf("write failed\n");
      return -1;
    }
  }
  else
  {
    if(extoll_read(src->u.rma.ex, cp_param->src_offset, cp_param->dest_offset, cp_param->bytes))
    {
      printf("read failed\n");
      return -1;
    }
  }
#endif
  return 0;
}

