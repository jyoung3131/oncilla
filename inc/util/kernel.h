/**
 * @file kernel.h
 *
 * @date 2012-02-14
 * @author Alex Merritt, merritt.alex@gatech.edu
 *
 * @brief Some definitions needed by other data structures I stole from the
 * Linux kernel sourcecode.
 */

#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h> // NULL

// from linux/stddef.h
#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

// from #include <linux/kernel.h>
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:  the type of the container struct this is embedded in.
 * @member:   the name of the member within the struct.
 *
 * Stolen from the linux source code, file include/linux/kernel.h
 * TODO Move this into another file. Someone might want to use container_of without the list
 * constructs in this file.
 */
#define container_of(ptr, type, member) ({                      \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})

#endif	/* KERNEL_H */
