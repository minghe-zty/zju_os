#include "stdint.h"
#include "printk.h"   // For printk function to print messages
#include "syscall.h"
#include "defs.h"
#include "proc.h"
extern char _sramdisk[], _eramdisk[];
extern void do_timer();
extern void clock_set_next_event();
extern struct task_struct *current;

/*void trap_handler(uint64_t scause, uint64_t sepc) {
    if ((scause >> 63)&&((scause & 0x7FFFFFFFFFFFFFFF) == 5)) {
        //printk("[S] Supervisor Mode Timer Interrupt\n");
        
        clock_set_next_event();  // Set the next timer interrupt
        do_timer();//refer
    } 
}*/

void trap_handler(uint64_t scause, uint64_t sepc, struct pt_regs *regs) {
    if ((scause >> 63)&&((scause & 0x7FFFFFFFFFFFFFFF) == 5)) {
        //printk("[S] Supervisor Mode Timer Interrupt\n");
        
        clock_set_next_event();  // Set the next timer interrupt
        do_timer();
    }else if(scause == 8){
        //针对系统调用这一类异常，我们需要手动完成 sepc + 4
        if(regs->reg[17] == SYS_WRITE){
            unsigned int curfd = regs->reg[10];
            const char* curbuf = regs->reg[11];
            size_t curcount = regs->reg[12];
            regs->reg[10] = sys_write(curfd, curbuf, curcount);
            //regs->reg[12] = sys_write(curfd, curbuf, curcount);
        }else if(regs->reg[17] == SYS_GETPID){
            regs->reg[10] = sys_getpid();
        }else if(regs->reg[17] == SYS_CLONE){
            regs->reg[10] = do_fork(regs);
        }else if(regs->reg[17] == SYS_READ) {
            unsigned int curfd = regs->reg[10];
            char* curbuf = (char *)regs->reg[11];
            size_t curcount = regs->reg[12];
            regs->reg[10] = sys_read(curfd, curbuf, curcount);
        }
        regs->sepc+=4;
    }else if(scause == 13||scause == 12||scause==15){
        printk("scause=%lx, sepc=%lx\n", scause, sepc);
        printk("from trap_handler to do_page_fault\n");
        do_page_fault(regs);
    }else{
        uint64_t stval = csr_read(stval);
        //Err("[S] Unhandled Exception: scause=%lx , sepc=%lx , stval=%lx \n", scause, sepc , stval);
    }
}

void do_page_fault(struct pt_regs *regs) {
    printk("do_page_fault\n");
    uint64_t cur_scause = csr_read(scause);
    uint64_t cur_stval = csr_read(stval);
    uint64_t cur_sepc = csr_read(sepc);
    Log("[PID = %d ] valid page fault %lx with scause %d,sepc:%lx\n",current->pid,cur_stval,cur_scause,cur_sepc);
    struct vm_area_struct* page_vm_add = find_vma(&current->mm, cur_stval);
    if (page_vm_add == NULL) {
        printk("illegal address, run time error.\n");
        Err("scause:%lx,stval:%lx,sepc:%lx\n",cur_scause,cur_stval,cur_sepc);
        return;
    }

    uint64_t vir_start_add = PGROUNDDOWN(cur_stval);
    uint64_t sz = PGROUNDUP(PGSIZE + cur_stval) - vir_start_add;
    uint64_t pa = (uint64_t)alloc_pages(sz / PGSIZE);
    uint64_t perm = 0x11;
    if (page_vm_add->vm_flags & VM_READ) {
        perm |= 0x02; 
    }
    if (page_vm_add->vm_flags & VM_WRITE) {
        perm |= 0x04; 
    }
    if (page_vm_add->vm_flags & VM_EXEC) {
        perm |= 0x08; 
    }
    perm |= 0xd0;
    memset((void*)pa, 0, sz);

    if ((page_vm_add->vm_flags & VM_ANON) == 0) {
        uint64_t dis = (uint64_t)_sramdisk + page_vm_add->vm_pgoff;
        uint64_t offset = cur_stval - page_vm_add->vm_start;
        uint64_t redis = PGROUNDDOWN(dis + offset);
        memcpy((char *)pa, (char *)redis, PGSIZE);
        uint64_t page_all_address = page_vm_add->vm_start + page_vm_add->vm_filesz;
        if(vir_start_add < page_all_address){
            if (vir_start_add + PGSIZE > page_all_address) {  
                memset(pa + (page_all_address - vir_start_add), 0, (PGSIZE + vir_start_add - page_all_address));
            }
        }else if (vir_start_add >= page_all_address){
            memset(pa, 0, PGSIZE);
        }
    }
    uint64_t cur_pa = pa - PA2VA_OFFSET;
    create_mapping(current->pgd, vir_start_add, cur_pa , PGSIZE, perm);
    printk("do_page_fault end\n");
}