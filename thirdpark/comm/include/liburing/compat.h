/* SPDX-License-Identifier: MIT */
#ifndef LIBURING_COMPAT_H
#define LIBURING_COMPAT_H

// #include <linux/time_types.h>
// #include <linux/openat2.h>
#include <sys/time.h>

#if __has_include(<linux/time_types.h>)
/* Newer kernel versions have this header which defines __kernel_timespec */
#include <linux/time_types.h>
#else
/* Simple fallback: __kernel_time_t/long in user space; long is sufficient */
typedef long __kernel_time_t;
typedef long __kernel_long_t;
struct __kernel_timespec {
	__kernel_time_t tv_sec;
	__kernel_long_t tv_nsec;
};
#endif

#if __has_include(<linux/openat2.h>)
#include <linux/openat2.h>
#else
/* minimal fallback for openat2 syscall */
struct open_how {
	__u64 flags;   /* O_* flags */
	__u64 mode;	   /* permission bits for new files */
	__u64 resolve; /* RESOLVE_* flags */
};
#define RESOLVE_FLAGS_CLEANPATH 0x00000001ULL
#define RESOLVE_FLAGS_NO_MAGICLINKS 0x00000002ULL
#endif

#endif
