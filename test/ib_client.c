#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <debug.h>
#include <time.h>

#include <io/rdma.h>
#include "../src/rdma.h"
#include <math.h>

static char *serverIP = NULL;

static ib_t setup(struct ib_params *p)
{
    ib_t ib;

    if (ib_init())
        return (ib_t)NULL;

    if (!(ib = ib_new(p)))
        return (ib_t)NULL;

    //Don't time here due to blocking statements
    if (ib_connect(ib, false/*is client*/))
        return (ib_t)NULL;

    return ib;
}

//Return 0 on success and 1 on failure
static int teardown(ib_t ib)
{
    int ret = 0;


    if (ib_disconnect(ib, false/*is client*/))
      ret = 1;
   
    //Free the IB structure
    if(ib_free(ib))
      ret = -1;

    return ret;
}

/* Does simple allocation test - for testing setup times*/
static int alloc_test(long long unsigned int size_B)
{
    ib_t ib;
    struct ib_params params;
    unsigned int *buf = NULL;
    //size_t count = size; // (1 << 10);
    unsigned long long num_bufs_to_alloc = size_B / sizeof(*buf);
    printf("Size of buf is %lu B so we allocate %llu buffers for a total of %llu B\n", sizeof(*buf), num_bufs_to_alloc, size_B);


    if (!(buf = calloc(num_bufs_to_alloc, sizeof(*buf))))
        return -1;

    params.addr     = serverIP;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = num_bufs_to_alloc;

    if (!(ib = setup(&params)))
        return -1;

    if(teardown(ib) != 0)
          return -1;
    
    /*Return 0 on succes*/
    return 0;

}

/* write/read to/from remote memory timing test. */
static int read_write_bw_test()
{

    ib_t ib;
    struct ib_params params;
    char *buf = NULL;

    unsigned long long count = pow(2,32)+1;
    unsigned long long len= count*sizeof(*buf);
    long long unsigned int size_B=64;
    long long unsigned int size_B2= size_B;

    if (!(buf = calloc(count, sizeof(*buf)))){
	    printf("memory allocation failed\n");
            return -1;
    }
    printf("size of count: %llu\n", count);
    printf("size of *buf : %lu\n", sizeof(*buf));

    params.addr     = serverIP;
    params.port     = 23456;
    params.buf      = buf;
    params.buf_len  = len;
    printf("Setting up\n");
    if (!(ib = setup(&params))){
        printf("Setup failed\n");
	    return -1;
    }
    printf("Setup done\n");

  while(size_B<=pow(2,32)){
	len=size_B;
	printf("------- %llu bytes -------\n", size_B);
 
	if(ib_write(ib, 0, 0, len)||ib_poll(ib))
	{
	    printf("write failed\n");
	    return -1;
	}
	memset(buf, 0, len);
    	size_B*=2;
	}
	// read back and wait for completion
      while(size_B2<=pow(2,32)){
	printf("------- %llu bytes -------\n", size_B2);
	len=size_B2;
	if(ib_read(ib, 0, 0, len)||ib_poll(ib))
	{
	    printf("read failed\n");
	    return -1;
	}
	
	size_B2*=2;
	}
	//Perform teardown
	if(teardown(ib) != 0){
	  printf("tear down error\n");
	  return -1;
	}
	//double the buffer size
    

    return 0; /* test passed */
}

/* Does a simple write/read to/from remote memory. */
static int one_sided_test(long long unsigned int size_B)
{
    ib_t ib;
    struct ib_params params;
    unsigned int *buf = NULL;
    size_t count = size_B; // (1 << 10);
    size_t len = count;// * sizeof(*buf);
    size_t i;

    if (!(buf = calloc(count, sizeof(*buf))))
        return -1;

    params.addr     = serverIP;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = len;

    if (!(ib = setup(&params)))
        return -1;

    for (i = 0; i < count; i++)
        buf[i] = 0xdeadbeef;

    /* send and wait for completion */
    if (ib_write(ib, 0, 0, len) || ib_poll(ib))
        return -1;

    memset(buf, 0, len);

    /* read back and wait for completion */
    if (ib_read(ib, 0, 0, len) || ib_poll(ib))
        return -1;

    for (i = 0; i < count; i++)
        if (buf[i] != 0xdeadbeef)
            return -1;

    //Perform teardown
    if(teardown(ib) != 0)
      return -1;



    return 0; /* test passed */
}

/* Allocate one string, server will allocate an array of strings. We update ours
 * and write it into one of the server's. Server sees this, then updates another
 * string in the array; we periodically pull and return when we see the change.
 */
static int buffer_size_mismatch_test(void)
{
    ib_t ib;
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

    /* send 'hello' to second index (third string) */
    /* wait for 'nice to meet you' in seventh (last) index */
    /* write back, releasing server */

    strncpy(buf->str, "hello", strlen("hello") + 1);
    if (ib_write(ib, (2 * len), 0,  len) || ib_poll(ib))
        return -1;

    times = 1000;
    char resp[] = "nice to meet you";
    do {
        usleep(500);
        if (ib_read(ib, (7 * len), 0,  len) || ib_poll(ib))
            return -1;
        is_equal = (strncmp(buf->str, resp, strlen(resp)) != 0);
    } while (--times > 0 && !is_equal);

    buf->str[0] = '\0';
    if (ib_write(ib, 0, 0, len) || ib_poll(ib))
        return -1;

    //Perform teardown
    if(teardown(ib) != 0)
      return -1;

    return 0;
}

// TODO bandwidth test

// TODO Multiple connections test (many clients)

// TODO Multiple regions test (one client many MRs)

int main(int argc, char *argv[])
{
    if (argc != 4) {
usage:
        fprintf(stderr, "Usage: %s <server_ip=10.0.0.[1=ifrit or 2=shiva]> <test_num> <alloc_size_MB>\n"
                "\ttest_num: 0 = one-sided; 1 = buffer mismatch; 2 = alloc; 3 = read/write BW test\n"
                "\talloc_size: can be specified in any positive decimal format\n", argv[0]);
        return -1;
    }
    serverIP = argv[1];

    //Convert the double value for MB input to bytes
    double reg_size_MB = strtod(argv[3], 0);
    uint64_t reg_size_B = (uint64_t)(reg_size_MB*pow(2,20));

    if(reg_size_MB > 8000.0)
    {
      printf("Please pass a data size of less than 8000 MB\n");
      return -1;
    }

    switch (atoi(argv[2])) {
    case 0:
        
        printf("Running one-sided test with buffer size %4f MB and %lu B\n",reg_size_MB, reg_size_B);
        if (one_sided_test(reg_size_B)) {
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
	printf("Running read/write BW test\n");
	if(read_write_bw_test()){
	    fprintf(stderr, "FAIL: read/write BW test\n");
	    return -1;
	} else
	    printf("pass: read/write BW test\n");
	break;
    default:
        goto usage; /* >:) */
    }
    return 0;
}
