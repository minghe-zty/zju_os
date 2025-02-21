#include<defs.h>
#include<string.h>
#include <stdint.h>
#include"printk.h"
#define OPENSBI_SIZE (0x200000)
/* early_pgtbl: 用于 setup_vm 进行 1GiB 的映射 */
uint64_t early_pgtbl[512] __attribute__((__aligned__(0x1000)));
// 调用打印函数
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);

void setup_vm() {
    /* 
     * 1. 由于是进行 1GiB 的映射，这里不需要使用多级页表 
     * 2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
     *     high bit 可以忽略
     *     中间 9 bit 作为 early_pgtbl 的 index
     *     低 30 bit 作为页内偏移，这里注意到 30 = 9 + 9 + 12，即我们只使用根页表，根页表的每个 entry 都对应 1GiB 的区域
     * 3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    **/
    //1. 由于是进行 1GiB 的映射，这里不需要使用多级页表 
    //printk("setup_vm in\n");
    memset(early_pgtbl, 0x0, PGSIZE);
    uint64_t pa = PHY_START,va = PHY_START;    
    early_pgtbl[va >> 30 &0x01ff] = (pa >> 12) << 10 |15;
    va = pa + (uint64_t)PA2VA_OFFSET;
    early_pgtbl[va >> 30 & 0x1ff] = (pa >> 12) << 10 |15;
    //printk("setup_vm out\n");
}




uint64_t swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern uint64_t _skernel,_stext, _srodata, _sdata, _sbss;

void setup_vm_final(void) {
    printk("setup_vm_final\n");
    memset(swapper_pg_dir, 0x0, PGSIZE);
    uint64_t va_text = (uint64_t)&_stext;
    uint64_t pa_text = va_text - PA2VA_OFFSET;
    uint64_t sz_text =(uint64_t)&_srodata - (uint64_t)&_stext;
    create_mapping(swapper_pg_dir,va_text,pa_text,sz_text,0xcb);

    uint64_t va_rodata = (uint64_t)&_srodata;
    uint64_t pa_rodata = va_rodata - PA2VA_OFFSET;
    uint64_t sz_rodata =(uint64_t)&_sdata - (uint64_t)&_srodata;
    create_mapping(swapper_pg_dir,va_rodata,pa_rodata,sz_rodata,0xc3);

    uint64_t va_data = (uint64_t)&_sdata;
    uint64_t pa_data = va_data - PA2VA_OFFSET;
    uint64_t sz_data = PHY_SIZE - ((uint64_t)&_sdata - (uint64_t)&_stext);
    create_mapping(swapper_pg_dir,va_data,pa_data,sz_data,0xc7);

    uint64_t t0 = swapper_pg_dir - PA2VA_OFFSET;
    t0 >>= 12;
    t0 = t0 |  0x4000000080000002; 
    asm volatile("csrw satp, %0" : : "r"(t0) : "memory");

    // flush TLBe

    asm volatile("sfence.vma zero, zero");
  
    return;
}


/* 创建多级页表映射关系 */
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm) {
    /*
     * pgtbl 为根页表的基地址
     * va, pa 为需要映射的虚拟地址、物理地址
     * sz 为映射的大小，单位为字节
     * perm 为映射的权限（即页表项的低 8 位）
     * 
     * 创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
     * 可以使用 V bit 来判断页表项是否存在
    **/
    uint64_t index0, index1, index2;
    uint64_t *page_address1, *page_address0;
    uint64_t next_page;

    for (uint64_t curr_address = va; curr_address < va + sz; curr_address += PGSIZE, pa += PGSIZE) {
        index2 = ((uint64_t)(curr_address) >> 30) & 0x1ff;
        index1 = ((uint64_t)(curr_address) >> 21) & 0x1ff;
        index0 = ((uint64_t)(curr_address) >> 12) & 0x1ff;

        if ((pgtbl[index2] & 1) == 0) {
            next_page = (uint64_t)kalloc();
            pgtbl[index2] = (((next_page - PA2VA_OFFSET) >> 12) << 10) | 1;
        }

        page_address1 = (uint64_t *)(((pgtbl[index2] >> 10) << 12) + PA2VA_OFFSET);
        if ((page_address1[index1] & 1) == 0) {
            next_page = (uint64_t)kalloc();
            page_address1[index1] = (((next_page - PA2VA_OFFSET) >> 12) << 10) | 1;
        }

        page_address0 = (uint64_t *)(((page_address1[index1] >> 10) << 12) + PA2VA_OFFSET);
        page_address0[index0] = perm | ((pa >> 12) << 10) | 1;
    }
    Log("root: %lx, [%lx, %lx) -> [%lx, %lx), perm: %x", pgtbl, pa, pa+sz, va, va+sz, perm);
}