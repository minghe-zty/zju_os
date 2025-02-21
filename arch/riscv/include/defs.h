#ifndef __DEFS_H__
#define __DEFS_H__

#include "stdint.h"
//#define sstatus 0x100

#define PHY_START 0x0000000080000000
#define PHY_SIZE 128 * 1024 * 1024 // 128 MiB，QEMU 默认内存大小
#define PHY_END (PHY_START + PHY_SIZE)

#define PGSIZE 0x1000 // 4 KiB
#define PGROUNDUP(addr) ((addr + PGSIZE - 1) & (~(PGSIZE - 1)))
#define PGROUNDDOWN(addr) (addr & (~(PGSIZE - 1)))
#define PA2VA_OFFSET (VM_START - PHY_START)

#define VM_START (0xffffffe000000000)
#define VM_END (0xffffffff00000000)
#define VM_SIZE (VM_END - VM_START)

#define PA2VA_OFFSET (VM_START - PHY_START)

#define USER_START (0x0000000000000000) // user space start virtual address
#define USER_END (0x0000004000000000) // user space end virtual address
/*#define csr_read(csr)                   \
  ({                                    \
    uint64_t __v;                       \
    _Static_assert(0, "Unimplemented"); \
    __v;                                \
  })*/

#define csr_read(csr)                           \
  ({                                            \
    uint64_t __v;                               \
    asm volatile("csrr %0, " #csr : "=r"(__v)); \
    __v;                                        \
  })


#define csr_write(csr, val)                                    \
  ({                                                           \
    uint64_t __v = (uint64_t)(val);                            \
    asm volatile("csrw " #csr ", %0" : : "r"(__v) : "memory"); \
  })

#endif
