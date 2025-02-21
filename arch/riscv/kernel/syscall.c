#include "syscall.h"
#include "printk.h"
#include "defs.h"
#include "string.h"
#include "vfs.h"
#include "sbi.h"
extern struct task_struct *current;
extern void __ret_from_fork();
extern struct task_struct* task[NR_TASKS];
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);
extern uint64_t swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern uint64_t nr_tasks;

uint64_t sys_write(unsigned int fd, const char* buf, size_t count) {
    //printk("sys_write fd:%d buf:%d \n",fd,buf);  
    size_t i;
    uint64_t ret = 0;
    if(fd == 1) {
        for(i = 0; i < count; i++) {
            printk("%c", buf[i]);
            ret++;
        }
    }else if(fd == 2) {
        for(i = 0; i < count; i++) {
            printk("%c", buf[i]);
            ret++;
        }
    }
    return ret;
}
char kuart_getchar() {
    char ret;
    while (1) {
        struct sbiret sbi_result = sbi_debug_console_read(1, ((uint64_t)&ret - PA2VA_OFFSET), 0);
        if (sbi_result.error == 0 && sbi_result.value == 1) {
            break;
        }
    }
    return ret;
}
uint64_t sys_read(unsigned int fd, char* buf, size_t count) {
    //printk("sys_read fd:%d buf:%d \n",fd,buf);  
    size_t i;
    uint64_t ret = 0;
    if(fd == 0) {
        for(i = 0; i < count; i++) {
            buf[i] = kuart_getchar();
            ret++;
            if(buf[i] == '\n') {
                break;
            }
        }
    }
    return ret;
}

uint64_t sys_getpid() {
    printk("sys_getpid %d\n",current->pid);
    return current->pid;
}


uint64_t cal_pa(uint64_t *pgtbl, uint64_t va) {
    uint64_t cur_index0,cur_index1,cur_index2;
    cur_index2 = 0x1ff & (va >> 30);
    cur_index1 = 0x1ff & (va >> 21);
    cur_index0 = 0x1ff & (va >> 12);
    uint64_t *offset = pgtbl;
    if ((offset[cur_index2] & 1) == 0) {
        return 0;
    }else{
        offset = (uint64_t *)(((offset[cur_index2] >> 10) << 12) + PA2VA_OFFSET);
    }
    if ((offset[cur_index1] & 1) == 0) {
        return 0;
    }else{
        offset = (uint64_t *)(((offset[cur_index1] >> 10) << 12) + PA2VA_OFFSET);
    }
    return (offset[cur_index0] >> 10) << 12;
}

     
uint64_t do_fork(struct pt_regs *regs){
    int new_pid = 0;
    uint64_t user_sp = 0;
    struct task_struct* child_task = (struct task_struct*)kalloc();
    struct vm_area_struct *cur_vma = current->mm.mmap;
    struct vm_area_struct *child_vma = (struct vm_area_struct *) kalloc(sizeof(struct vm_area_struct));
    memcpy(child_task,current,PGSIZE);
    printk("do_fork function\n");
    for(int i = 0;i < NR_TASKS;i++){
        if(task[i] == NULL){
            task[i] = child_task;
            new_pid = i;
            break;
        }
    }
    
    //创建一个新进程,拷贝内核栈（包括了 task_struct 等信息）
    if(child_task==NULL){
        printk("no room for child_task\n");
        return -1;
    }
    
    child_task->pid = new_pid;
    child_task->mm.mmap = NULL;
    child_task->pgd = (uint64_t *)kalloc();
    child_task->thread.ra = (uint64_t)__ret_from_fork;
    child_task->thread.sscratch = csr_read(sscratch);
    child_task->thread.sp = (uint64_t)child_task + ((uint64_t)regs & 0xfff);
    if(child_task->pgd == NULL){
        printk("no room for child_task pgd\n");
        return -1;
    }
    //拷贝内核页表 swapper_pg_dir
    memcpy(child_task->pgd, swapper_pg_dir, PGSIZE);
    uint64_t offset = (uint64_t)regs - (uint64_t)current;
    struct pt_regs *child_regs = (struct pt_regs *)((uint64_t)child_task + offset);
    child_regs->reg[10] = 0;
    child_regs->reg[2] = (uint64_t)child_regs;
    child_regs->sepc = regs->sepc + 4;
    *(uint64_t *)((uint64_t)regs + 9*8) = child_task->pid; 
    //遍历父进程 vma，并遍历父进程页表
    //将这个 vma 也添加到新进程的 vma 链表中
    //如果该 vma 项有对应的页表项存在（说明已经创建了映射），则需要深拷贝一整页的内容并映射到新页表中
    while (cur_vma!=NULL) {
        struct vm_area_struct *child_vma = (struct vm_area_struct *) kalloc(sizeof(struct vm_area_struct));
        child_vma ->vm_start = cur_vma -> vm_start;
        child_vma ->vm_end = cur_vma -> vm_end;
        child_vma ->vm_flags = cur_vma -> vm_flags;
        child_vma ->vm_pgoff = cur_vma -> vm_pgoff;
        child_vma ->vm_filesz = cur_vma -> vm_filesz;
        child_vma ->vm_mm = &(child_task->mm);
        child_vma ->vm_prev = NULL;
        if(child_task->mm.mmap != NULL){
            struct vm_area_struct *temp = child_task->mm.mmap;
            child_vma ->vm_next = temp;
            temp ->vm_prev = child_vma;            
        }else{ 
            child_vma ->vm_next = NULL;
        }
        child_task->mm.mmap = child_vma;
        uint64_t va = PGROUNDDOWN(cur_vma->vm_start);
        for(; va < PGROUNDUP(cur_vma->vm_end); va += PGSIZE){
            uint64_t pa = cal_pa(current->pgd, va);
            if(pa != 0){
                uint64_t child_page = alloc_page();
                uint64_t cur_pa = child_page - PA2VA_OFFSET;
                memcpy((char *)child_page, (char *)(pa + PA2VA_OFFSET), PGSIZE);
                if(cur_vma->vm_flags & VM_ANON){
                    create_mapping(child_task->pgd, va, cur_pa , PGSIZE, 0x17);
                    uint64_t sscratch = csr_read(sscratch);
                    user_sp = (sscratch & 0xfff) + va;
                }else{
                    create_mapping(child_task->pgd, va, cur_pa , PGSIZE, 0x1f);
                }
            }
        }
        cur_vma = cur_vma -> vm_next;
    }
    task[new_pid] = child_task;
    Log("[PID = %d] forked from [PID = %d]",child_task->pid,current->pid);
    return child_task->pid;
}