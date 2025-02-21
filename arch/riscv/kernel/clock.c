#include "sbi.h"
#include "stdint.h"

// QEMU 中时钟的频率是 10MHz，也就是 1 秒钟相当于 10000000 个时钟周期
uint64_t TIMECLOCK = 10000000;

uint64_t get_cycles() {
    // 编写内联汇编，使用 rdtime 获取 time 寄存器中（也就是 mtime 寄存器）的值并返回
    #if __riscv_xlen == 64
    uint64_t n;
        __asm__ __volatile__("rdtime %0": "=r"(n));
        return n;
    #else
    uint32_t l, h, t;
        __asm__ __volatile__(
            "1:\n"
            "rdtimeh %0\n"
            "rdtime %1\n"
            "rdtimeh %2\n"
            "bne %0, %2, 1b"
        : "=&r"(h), "=&r"(l), "=&r"(t));
        return l | ((uint64_t)h << 32) ;
    #endif
    //#error Unimplemented
}

void clock_set_next_event() {
    // 下一次时钟中断的时间点
    uint64_t next = get_cycles() + TIMECLOCK;
    sbi_set_timer(next);
    // 使用 sbi_set_timer 来完成对下一次时钟中断的设置
    //#error Unimplemented
}