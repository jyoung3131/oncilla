#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <debug.h>
#include <time.h>

#include <io/extoll.h>
#include "../src/extoll.h"
#include <util/timer.h>
#include <math.h>

static unsigned int server_node_id;
static unsigned int server_vpid;
static unsigned long long server_nla;

static extoll_t setup(struct extoll_params *p)
{
  extoll_t ex;

  if (extoll_init())
    return (extoll_t)NULL;

  if (!(ex = extoll_new(p)))
    return (extoll_t)NULL;

  if (extoll_connect(ex, false/*is client*/))
    return (extoll_t)NULL;

  return ex;
}

//Return 0 on success and 1 on failure
static int teardown(extoll_t ex)
{
  int ret = 0;

  TIMER_DECLARE1(ex_disconnect_timer);
  TIMER_START(ex_disconnect_timer);

  if (extoll_disconnect(ex, false/*is client*/))
    ret = 1;

#ifdef TIMING
  uint64_t extoll_teardown_ns = 0;
  TIMER_END(ex_disconnect_timer, extoll_teardown_ns);
  printf("[DISCONNECT] Time for extoll_disconnect: %lu ns\n", extoll_teardown_ns);
#endif

  //Free the EXTOLL structure
  if(extoll_free(ex))
    ret = -1;

  return ret;
}

/* Does simple allocation test - for testing setup times*/
static int alloc_test(long long unsigned int size_B)
{
  extoll_t ex;
  struct extoll_params params;

  printf("Allocating %llu bytes \n", size_B);

  //The extoll_client_connect function currently allocates memory 
  //if (!(buf = calloc(num_bufs_to_alloc, sizeof(*buf))))
  //    return -1;

  printf("Remote server connection - node: %d, vpid: %d, NLA %llx\n",server_node_id, server_vpid,server_nla);
  params.dest_node = server_node_id;
  params.dest_vpid = server_vpid;
  params.dest_nla = server_nla;
  params.buf_len  = size_B;

  if (!(ex = setup(&params)))
    return -1;

  if(teardown(ex) != 0)
    return -1;

  /*Return 0 on succes*/
  return 0;

}

/* write/read to/from remote memory timing test. */
static int read_write_bw_test(uint64_t max_size_B)
{
  extoll_t ex;
  struct extoll_params params;

  unsigned long long len = max_size_B+1;
  long long unsigned int size_B=64;
  long long unsigned int size_B2= size_B;

  printf("Remote server connection - node: %d, vpid: %d, NLA %llx\n",server_node_id, server_vpid,server_nla);
  params.dest_node = server_node_id;
  params.dest_vpid = server_vpid;
  params.dest_nla = server_nla;

  //Allocation is in the extoll_server_connect function
  params.buf_len  = len;
  printf("Setting up\n");
  if (!(ex = setup(&params))){
    printf("Setup failed\n");
    return -1;
  }
  printf("Setup done\n");

  while(size_B <= max_size_B){
    len=size_B;

    if(extoll_write(ex, 0, 0, len))
    {
      printf("write failed\n");
      return -1;
    }
    size_B*=2;
  }

  // Perform read test
  while(size_B2 <= max_size_B){
    len=size_B2;
    if(extoll_read(ex, 0, 0, len))
    {
      printf("read failed\n");
      return -1;
    }
    size_B2*=2;
  }
  //Perform teardown
  if(teardown(ex) != 0){
    printf("tear down error\n");
    return -1;
  }

  return 0; /* test passed */
}



/* Does a simple write/read to/from remote memory. */
static int one_sided_test()
{
  extoll_t ex;
  struct extoll_params params;
  size_t size_B = (1 << 10);
  size_t count = size_B/sizeof(uint32_t);
  size_t i;
  int ret = 0;

  //Initialize the buffer to all 0s and then set the buffer length
  memset(&params, 0, sizeof(struct extoll_params));

  params.buf_len  = size_B;

  if (!(ex = setup(&params)))
    return -1;

  uint32_t* buf_ptr = (uint32_t*)ex->rma_conn.buf;
  for (i = 0; i < count; i++)
    buf_ptr[i] = 1234;

  // send and wait for completion 
  if (extoll_write(ex, 0, 0, size_B))
    return -1;

  //Reset the buffer for read
  memset(buf_ptr, 0, size_B);

  // read back and wait for completion
  printf("Reading back data\n");
  if (extoll_read(ex, 0, 0, size_B))
    return -1;

  for (i = 0; i < count; i++)
  {
    printf("i %lu val %d\n",i, buf_ptr[i]);
    if (buf_ptr[i] != 1234)
    {
      printf("buf_ptr[%lu] mismatch: 0x%x\n", i, buf_ptr[i]);
      ret = -1;
    }
  }
  //Perform teardown
  if(teardown(ex) != 0)
    ret = -1;


  return ret; /* test passed */
}

/* Allocate one string, server will allocate an array of strings. We update ours
 * and write it into one of the server's. Server sees this, then updates another
 * string in the array; we periodically pull and return when we see the change.
 */
static int buffer_size_mismatch_test(void)
{
  /*extoll_t ex;
    struct ib_params params;
    struct {
    char str[32];
    } *buf = NULL;
    size_t len = sizeof(*buf);
    unsigned int times;
    bool is_equal;

    if (!(buf = calloc(1, len)))
    return -1;

    params.addr     = serverIP;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = len;

    if (!(ib = setup(&params)))
    return -1;

  // send 'hello' to second index (third string) 
  // wait for 'nice to meet you' in seventh (last) index 
  // write back, releasing server 

  strncpy(buf->str, "hello", strlen("hello") + 1);
  if (ib_write(ib, (2 * len), len) || ib_poll(ib))
  return -1;

  times = 1000;
  char resp[] = "nice to meet you";
  do {
  usleep(500);
  if (ib_read(ib, (7 * len), len) || ib_poll(ib))
  return -1;
  is_equal = (strncmp(buf->str, resp, strlen(resp)) != 0);
  } while (--times > 0 && !is_equal);

  buf->str[0] = '\0';
  if (ib_write(ib, 0, len) || ib_poll(ib))
  return -1;

  //Perform teardown
  if(teardown(ib) != 0)
  return -1;
   */
  return 0;
}

// TODO bandwidth test

// TODO Multiple connections test (many clients)

// TODO Multiple regions test (one client many MRs)

int main(int argc, char *argv[])
{
  if (argc != 6) {
usage:
    fprintf(stderr, "Usage: %s <test_num> <server_node_id> <server_dest_vpid> <server_nla> <alloc_size_MB>\n"
        "\ttest_num: 0 = one-sided; 1 = buffer mismatch; 2 = alloc; 3 read/write BW test\n"
        "\talloc_size: can be specified in any positive decimal format\n", argv[0]);
    return -1;
  }
  server_node_id = strtol(argv[2],0,0);
  server_vpid = strtol(argv[3],0,0);
  server_nla = strtol(argv[4],0,0);

  //Convert the double value for MB input to bytes
  double reg_size_MB = strtod(argv[5], 0);
  uint64_t reg_size_B = (uint64_t)(reg_size_MB*pow(2,20));

  if(reg_size_MB > 4000.0)
  {
    printf("Please pass a data size of less than 4000 MB\n");
    return -1;
  }

  switch (atoi(argv[1])) {
    case 0:

      printf("Running one-sided test\n");
      if (one_sided_test()) {
        fprintf(stderr, "FAIL: one_sided_test\n");
        return -1;
      } else
        printf("pass: one_sided_test\n");
      break;
    case 1:
      if (buffer_size_mismatch_test()) {
        fprintf(stderr, "FAIL: buffer_size_mismatch_test\n");
        return -1;
      } else
        printf("pass: buffer_size_mismatch_test\n");
      break;
    case 2:
      printf("Running allocation test with buffer size %4f MB and %lu B\n",reg_size_MB, reg_size_B);
      if(alloc_test(reg_size_B)) {
        fprintf(stderr, "FAIL: alloc_test\n");
        return -1;
      } else 
        printf("pass: alloc_test\n");
      break;
    case 3:
      printf("Running read/write BW test \n");
      if(read_write_bw_test(reg_size_B)){
        fprintf(stderr, "FAIL: read/write BW test\n");
        return -1;
      } else
        printf("pass: read/write BW test\n");

    default:
      goto usage; /* >:) */
  }
  return 0;
}
