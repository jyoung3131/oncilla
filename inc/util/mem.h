/**
   * @file mem.h
   *
   * @date 2013-03-24
   * @author Jeff Young <jyoung9@gatech.edu>
   *
   * @brief Some definitions needed by other data structures I stole from the
   * Linux kernel sourcecode.
**/

#ifndef UTIL_MEM_H
#define UTIL_MEM_H


#include <sys/sysinfo.h>    // sysinfo
#include <stdio.h>
#include <unistd.h>     // sysconf

uint64_t get_free_mem()
{
  struct sysinfo info;

  uint64_t free_bytes = 0;

  if (sysinfo(&info) != 0)
    return 0;

  free_bytes = info.freeram;
  printd("Free RAM: %lf MB\n", ((double)(info.freeram))/((double)(2<<20)));

  return free_bytes;
}

#endif
