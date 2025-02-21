#include "stdint.h"
#include "sbi.h"

/*sbi_ecall 函数中，需要完成以下内容：

将 eid（Extension ID）放入寄存器 a7 中，fid（Function ID）放入寄存器 a6 中，将 arg[0-5] 放入寄存器 a[0-5] 中。
使用 ecall 指令。ecall 之后系统会进入 M 模式，之后 OpenSBI 会完成相关操作。
OpenSBI 的返回结果会存放在寄存器 a0，a1 中，其中 a0 为 error code，a1 为返回值，我们用 sbiret 来接受这两个返回值。*/

struct sbiret sbi_ecall(uint64_t eid, uint64_t fid,
                        uint64_t arg0, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    struct sbiret ret;
    __asm__ volatile(
        "mv a7, %[eid]\n"
        "mv a6, %[fid]\n"
        "mv a0, %[arg0]\n"
        "mv a1, %[arg1]\n"
        "mv a2, %[arg2]\n"
        "mv a3, %[arg3]\n"
        "mv a4, %[arg4]\n"
        "mv a5, %[arg5]\n"
        "ecall\n"
        "mv %[ret_error], a0\n"
        "mv %[ret_value], a1\n"
        : [ret_error] "=r"(ret.error), [ret_value] "=r"(ret.value)
        : [arg0] "r"(arg0), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3), [arg4] "r"(arg4), [arg5] "r"(arg5), [fid] "r"(fid), [eid] "r"(eid)
        :"memory"
        );
    return ret;
}
struct sbiret sbi_debug_console_write_byte(uint8_t byte) {
    return sbi_ecall(0x4442434E, 0x2, (uint8_t)byte, 0, 0, 0, 0, 0);
}

struct sbiret sbi_debug_console_read(uint64_t num_bytes, uint64_t base_addr_lo, uint64_t base_addr_hi){
    return sbi_ecall(0x4442434E, 0x1, num_bytes, base_addr_lo, base_addr_hi, 0, 0, 0);
}

struct sbiret sbi_system_reset(uint32_t reset_type, uint32_t reset_reason) {
    return sbi_ecall(0x53525354, 0x0, (uint32_t)reset_type, (uint32_t)reset_reason, 0, 0, 0, 0);
}

struct sbiret sbi_set_timer(uint64_t stime_value){
    struct sbiret ret;
    register uint64_t a0 asm("a0") = stime_value;  // Set argument a0
    register uint64_t a7 asm("a7") = 0x00;         // SBI function ID for set timer
    asm volatile (
        "ecall"                                     // Make the SBI call
        :                                           // No outputs
        : "r"(a0), "r"(a7)                          // Inputs
        : "memory"                                  // Clobbers
    );
    return ret;
}

/*void sbi_set_timer(uint64_t stime_value) {
    // SBI call to set the timer
    register uint64_t a0 asm("a0") = stime_value;  // Set argument a0
    register uint64_t a7 asm("a7") = 0x00;         // SBI function ID for set timer
    asm volatile (
        "ecall"                                     // Make the SBI call
        :                                           // No outputs
        : "r"(a0), "r"(a7)                          // Inputs
        : "memory"                                  // Clobbers
    );
}*/