#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "stdint.h"
#include "stddef.h"
#include "proc.h"
#include "vfs.h"
#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_CLONE 220
#define SYS_READ 63

uint64_t sys_write(unsigned int fd, const char* buf, size_t count);
uint64_t sys_getpid();
uint64_t do_fork(struct pt_regs *regs);

#endif