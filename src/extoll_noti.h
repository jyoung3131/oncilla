/* file: extoll_noti.h
  * author: Jeff Young <jyoung9@gatech.edu>
  * desc: Internal data structures used to get around teardown issues
  * with pinned pages and RMA2. 
  *
*/

#ifndef __EXTOLL_NOTI_H__
#define __EXTOLL_NOTI_H__
//stores the location we want to jump back to from
//the notification loop - making this static might cause
//a segfault - not yet sure why
jmp_buf jmp_noti_buf;
volatile int noti_loop;

#endif
