#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <io/extoll.h>
#include <math.h>
#include "../src/extoll.h"

static extoll_t setup(struct extoll_params *p)
{
  extoll_t ex = NULL;

  if (extoll_init())
    return (extoll_t)NULL;

  if (!(ex = extoll_new(p)))
    return (extoll_t)NULL;

  //We don't time this function because it has several blocking
  //statements within which provide a false picture of timing.
  if (extoll_connect(ex, true/*is server*/))
    return (extoll_t)NULL;

  return ex;
}

//Return 0 on success and -1 on failure
static int teardown(extoll_t ex)
{
  int ret = 0;

  if (extoll_disconnect(ex, true/*is server*/))
    ret = -1;

  //Free the IB structure
  if(extoll_free(ex))
    ret = -1;

  return 0;
}


/* Does simple allocation test - for testing setup times*/
static int alloc_test(long long unsigned int size_B)
{
  extoll_t ex;
  struct extoll_params params;

  printf("Size of buffer is %llu  B\n", size_B);

  //extoll_server_connect allocates memory

  //Initialize the buffer to all 0s and then set the buffer length
  memset(&params, 0, sizeof(struct extoll_params));
  //Use bytes for the buffer length here
  params.buf_len  = size_B;

  if (!(ex = setup(&params)))
    return -1;

  //Wait for the client to connect and perform read/write operations
  //This is mainly used to keep the allocation active for this test since
  //we aren't doing reads/writes here.
  extoll_notification(ex);

  if(teardown(ex) != 0)
    return -1;

  /*Return 0 on success*/
  return 0;

}


static int one_sided_test(void)
{
  extoll_t ex;
  struct extoll_params params;
  int ret_val = 0;

  size_t size_B = (1 << 10);
  size_t count = size_B/sizeof(uint32_t);

  printf("Size of buffer is %lu  B\n", size_B);

  //extoll_server_connect allocates memory

  //Initialize the buffer to all 0s and then set the buffer length
  memset(&params, 0, sizeof(struct extoll_params));
  //Use bytes for the buffer length here
  params.buf_len  = size_B;

  if (!(ex = setup(&params)))
    return -1;

  uint32_t* buf_ptr = (uint32_t*)ex->rma.buf;
  //Remember to memset using bytes
  memset(buf_ptr, 1234, size_B);

  //Wait for the client to connect and perform read/write operations
  //This is mainly used to keep the allocation active for this test since
  //we aren't doing reads/writes here.
  extoll_notification(ex);
  
  uint32_t i ;
  for (i = 0; i < count; i++)
    if (buf_ptr[i] != 1234)
    {
      printf("buf_ptr[%d] mismatch=%x\n", i, buf_ptr[i]);
      ret_val = -1;
    }
  
  if(teardown(ex) != 0)
    return -1;

  /*Return 0 on success*/
  return ret_val;

}

static int buffer_size_mismatch_test(void)
{
  /*extoll_t ex;
  struct ex_params params;
  struct {
    char str[32];
  } *buf = NULL;
  size_t count = 8;
  size_t len = count * sizeof(*buf);
  unsigned int times;
  bool is_equal;

  if (!(buf = calloc(count, sizeof(*buf))))
    return -1;

  params.buf      = buf;
  params.buf_len  = len;

  if (!(ib = setup(&params)))
    return -1;

  // wait for client to update us 
  times = 1000;
  char recv[] = "hello";
  char resp[] = "nice to meet you";
  do {
    usleep(500);
    is_equal = (strncmp(buf[2].str, recv, strlen(recv)) == 0);
  } while (--times > 0 && !is_equal);
  if (times == 0)
    return -1;

  // create response. client will be pulling 
  strncpy(buf[7].str, resp, strlen(resp) + 1);


  if(teardown(ex) != 0)
    return -1;
 */
  /*Return 0 on success*/
  return 0;


}

// bandwidth test
/* read / write to/from memory */
static int read_write_bw_test(uint64_t size_B)
{

  extoll_t ex;
  struct extoll_params params;
  //Allocate 2 GB of data
  //size_t size_B = pow(2,30)+1;

  printf("Daemon allocating %lu B or %3f GB of memory\n", size_B, ((double)size_B/pow(2,30.0)));

  //extoll_server_connect allocates memory

  //Initialize the buffer to all 0s and then set the buffer length
  memset(&params, 0, sizeof(struct extoll_params));
  //Use bytes for the buffer length here
  params.buf_len  = size_B;

  if (!(ex = setup(&params)))
    return -1;

  //Wait for the client to connect and perform read/write operations
  //This is mainly used to keep the allocation active for this test since
  //we aren't doing reads/writes here.
  extoll_notification(ex);

  if(teardown(ex) != 0)
    return -1;

  /*Return 0 on success*/
  return 0;

}



// TODO Multiple connections test

// TODO Multiple regions test

int main(int argc, char *argv[])
{
  if (argc != 3) {
usage:
    fprintf(stderr, "Usage: %s <test_num> <alloc_size_MB>\n"
        "\ttest_num: 0 = one-sided; 1 = buffer mismatch; 2 = alloc; 3 = read/write bandwidth daemon\n"
        "\talloc_size: can be specified in any positive decimal format\n", argv[0]);

    return -1;
  }

  double reg_size_MB = strtod(argv[2], 0);
  uint64_t reg_size_B = (uint64_t)(reg_size_MB*pow(2,20));

  if(reg_size_MB > 4000.0)
  {
    printf("Please pass a data size of less than 8000 MB\n");
    return -1;
  }

  switch (atoi(argv[1])) {
    case 0:
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
      printf("Running daemon for read/write bandwidth test\n");
      if(read_write_bw_test(reg_size_B)){
          fprintf(stderr, "FAIL: read/write bandwidth test\n");
          return -1;
      } else
          printf("PASS: read/write bandwidth test\n");
      break;

    default:
      goto usage;
  }
  return 0;
}
