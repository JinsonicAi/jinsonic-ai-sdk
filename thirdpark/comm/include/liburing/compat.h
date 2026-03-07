/* SPDX-License-Identifier: MIT */
#ifndef LIBURING_COMPAT_H
#define LIBURING_COMPAT_H

// #include <linux/time_types.h>
// #include <linux/openat2.h>
#include <sys/time.h>

#if __has_include(<linux/time_types.h>)
/* 内核新版本会有这个头，它定义了 __kernel_timespec */
#include <linux/time_types.h>
#else
/* 简单补一个：__kernel_time_t/long 在 user space 用 long 够用 */
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
