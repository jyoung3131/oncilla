/* file: extoll_noti.h
  * author: Jeff Young <jyoung9@gatech.edu>
  * desc: internal data structures for the EXTOLL interface
  *
*/

#ifndef __EXTOLL_NOTI_H__
#define __EXTOLL_NOTI_H__
//stores the location we want to jump back to from
//the notification loop
static jmp_buf jmp_noti_buf;
static volatile int noti_loop;

static void sighandler(int sig)
{
    printf("Received signal %d - breaking out of the loop\n",sig);
    noti_loop = 0;

    //Jump back to src/extoll.c:extoll_notification initiate EXTOLL teardown
    longjmp(jmp_noti_buf,1);
}

#endif
