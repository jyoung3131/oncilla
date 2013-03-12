#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <io/rdma.h>
#include <util/timer.h>
#include <math.h>
#include "../src/rdma.h"

static ib_t setup(struct ib_params *p)
{
	ib_t ib = NULL;

	if (ib_init())
		return (ib_t)NULL;

	if (!(ib = ib_new(p)))
		return (ib_t)NULL;

  //We don't time this function because it has several blocking
  //statements within which provide a false picture of timing.
  if (ib_connect(ib, true/*is server*/))
	return (ib_t)NULL;

	return ib;
}

//Return 0 on success and -1 on failure
static int teardown(ib_t ib)
{
  int ret = 0;
  
  uint64_t ib_teardown_ns = 0;
  TIMER_DECLARE1(ib_disconnect_timer);
  TIMER_START(ib_disconnect_timer);

	if (ib_disconnect(ib, true/*is server*/))
		ret = -1;

  TIMER_END(ib_disconnect_timer, ib_teardown_ns);
  printf("[DISCONNECT] Time for ib_disconnect: %lu ns\n", ib_teardown_ns);
  //Destroy the timer once we are done with it
  //TIMER_DESTROY(ib_disconnect_timer);

  //Free the IB structure
  if(ib_free(ib))
    ret = -1;

	return 0;
}


/* Does simple allocation test - for testing setup times*/
static int alloc_test(long long unsigned int size_B)
{
	ib_t ib;
	struct ib_params params;
	unsigned int *buf = NULL;
	unsigned long long num_bufs_to_alloc = size_B / sizeof(*buf);
	printf("Size of buf is %lu B so we allocate %llu buffers for a total of %llu B\n", sizeof(*buf), num_bufs_to_alloc, size_B);

	if (!(buf = calloc(num_bufs_to_alloc, sizeof(*buf))))
		return -1;
	
	params.addr     = NULL;
	params.port     = 12345;
	params.buf      = buf;
	params.buf_len  = num_bufs_to_alloc;

	if (!(ib = setup(&params)))
		return -1;

  if(teardown(ib) != 0)
    return -1;

	/*Return 0 on success*/
	return 0;

}

/* read / write to/from memory timing test */
static int read_write_test(void){

	ib_t ib;
	struct ib_params params;
	char *buf = NULL;
	size_t count = pow(2,32)+1;
	size_t len = count * sizeof(*buf);

	if (!(buf = calloc(count, sizeof(*buf))))
		return -1;

  printf("Daemon allocating %lu B of memory\n", len);

	params.addr     = NULL;
	params.port     = 23456;
	params.buf      = buf;
	params.buf_len  = len;
	
	if (!(ib = setup(&params))){
		printf("setup failed\n");
		return -1;
	}
	while((getchar()!=EOF)){

	}
	

	  if(teardown(ib) != 0)
	  return -1;
	
	return 0;
}
static int one_sided_test(void)
{
	ib_t ib;
	struct ib_params params;
	unsigned int *buf = NULL;
	size_t count = 1000000000;
	size_t len = count * sizeof(*buf);

	if (!(buf = calloc(count, sizeof(*buf))))
		return -1;

	params.addr     = NULL;
	params.port     = 12345;
	params.buf      = buf;
	params.buf_len  = len;

	if (!(ib = setup(&params)))
		return -1;

	/* wait for client to write entire buffer */
	while (buf[count - 1] == 0)
		usleep(500);

	/* verify all elements have been updated */
	size_t i;
	for (i = 0; i < count; i++)
		if (buf[i] != 0xdeadbeef)
			fprintf(stderr, "x");

	/* XXX need to implement teardown() */

	return 0;
}

/* Allocate an array of strings. Client will update one of them, we respond by
 * updating another. Client will spin until it sees our changes, by periodically
 * reading from our buffer.
 */
static int buffer_size_mismatch_test(void)
{
	ib_t ib;
	struct ib_params params;
	struct {
		char str[32];
	} *buf = NULL;
	size_t count = 8;
	size_t len = count * sizeof(*buf);
	unsigned int times;
	bool is_equal;

	if (!(buf = calloc(count, sizeof(*buf))))
		return -1;

	params.addr     = NULL;
	params.port     = 12345;
	params.buf      = buf;
	params.buf_len  = len;

	if (!(ib = setup(&params)))
		return -1;

	/* wait for client to update us */
	times = 1000;
	char recv[] = "hello";
	char resp[] = "nice to meet you";
	do {
		usleep(500);
		is_equal = (strncmp(buf[2].str, recv, strlen(recv)) == 0);
	} while (--times > 0 && !is_equal);
	if (times == 0)
		return -1;

	/* create response. client will be pulling */
	strncpy(buf[7].str, resp, strlen(resp) + 1);

	/* client will write, telling us they got the response */
	times = 1000;
	while (--times > 0 && buf[0].str[0] != '\0')
		usleep(500);
	if (times == 0)
		return -1;

	/* XXX need to implement teardown() */

	return 0;
}

// TODO bandwidth test

// TODO Multiple connections test

// TODO Multiple regions test

int main(int argc, char *argv[])
{
	if (argc != 3) {
usage:
		fprintf(stderr, "Usage: %s <test_num> <alloc_size_MB>\n"
       "\ttest_num: 0 = one-sided; 1 = buffer mismatch; 2 = alloc\n"
       "\talloc_size: can be specified in any positive decimal format\n", argv[0]);

		return -1;
	}

	double reg_size_MB = strtod(argv[2], 0);
	uint64_t reg_size_B = (uint64_t)(reg_size_MB*pow(2,20));

  if(reg_size_MB > 8000.0)
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
			printf("Running read/write test with starting buffer size %4f MB and %lu B\n", reg_size_MB, reg_size_B);
			if(read_write_test()){
			    fprintf(stderr, "FAIL: read/write test\n");
			    return -1;
			} else
			    printf("PASS: read/write test\n");		
			break;
    default:
      goto usage;
	}
  return 0;
}
