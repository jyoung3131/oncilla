/**
 * @file compiler.h
 *
 * @date October 05, 2011
 * @author Alex Merritt, merritt.alex@gatech.edu
 *
 * @brief This file defines some compiler-specific optimizations, similar to the
 * one provided in the Linux kernel.
 *
 * On this day, Steve Jobs passed away. Rest in peace.
 */

#ifndef __COMPILER_H
#define __COMPILER_H

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#endif /* __COMPILER_H */
