#include "mm.h"
#include "defs.h"
#include "proc.h"
#include "stdlib.h"
#include "printk.h"
#include "elf.h"
#include <string.h>

extern void __dummy();
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);
extern uint64_t swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern char _sramdisk[], _eramdisk[];
extern char uapp_start[];
extern char uapp_end[];
struct files_struct *file_init();
struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 task_struct
struct task_struct *task[NR_TASKS]; // 线程数组，所有的线程都保存在此
uint64_t nr_tasks=0;
struct vm_area_struct *find_vma(struct mm_struct *mm, uint64_t addr) {
    struct vm_area_struct *vma = mm->mmap;
    while (vma != NULL) {
        if (addr >= vma->vm_start && addr < vma->vm_end) {
            break; 
        }else{
            vma = vma->vm_next;
        }
    }
    return vma;
}

uint64_t do_mmap(struct mm_struct *mm, uint64_t addr, uint64_t len, uint64_t vm_pgoff, uint64_t vm_filesz, uint64_t flags) {
    uint64_t vir_start_add = PGROUNDDOWN(addr); 
    uint64_t vir_end_add = PGROUNDUP(addr + len);
    struct vm_area_struct *prev_vma = NULL;
    struct vm_area_struct *cur_vma = (struct vm_area_struct *)kalloc(sizeof(struct vm_area_struct));
    struct vm_area_struct *next_vma = mm -> mmap;
    while( next_vma != NULL){
        if(vir_end_add > next_vma -> vm_start){
            prev_vma = next_vma;
            next_vma = next_vma -> vm_next;
        }else{
            break;
        }
    }
    cur_vma -> vm_start = vir_start_add;
    cur_vma -> vm_end = vir_end_add;
    cur_vma -> vm_mm = mm;
    cur_vma -> vm_flags = flags;
    cur_vma -> vm_pgoff = vm_pgoff;
    cur_vma -> vm_filesz = vm_filesz;
    cur_vma -> vm_prev = prev_vma;
    cur_vma -> vm_next = next_vma;
    if(prev_vma == NULL){
        mm -> mmap = cur_vma;
    }else{
        prev_vma -> vm_next = cur_vma;
        if(next_vma){
            next_vma -> vm_prev = cur_vma;
        }
    }
    return addr;
}

void load_program(struct task_struct *task) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)_sramdisk;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(_sramdisk + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        Elf64_Phdr *phdr = phdrs + i;
        if (phdr->p_type == PT_LOAD) {
            uint64_t perm = VM_READ | VM_WRITE | VM_EXEC | 0xd0;
            do_mmap(&task->mm, PGROUNDDOWN(phdr->p_vaddr), phdr->p_memsz, phdr->p_offset, phdr->p_filesz, perm);
        }
    }
    task->thread.sepc = ehdr->e_entry;
}


void task_init() {
    srand(2024);
    idle = (struct task_struct *)kalloc();
    if (idle == NULL) {
        printk("Failed to allocate memory for idle task\n");
        return;
    }
    idle -> state = TASK_RUNNING;
    idle -> counter = 0;
    idle -> priority = 0;
    idle -> pid =0;
    current = idle;
    task[0] = idle;
    for(int i = 1 ;i < 2; i++){
        task[i] = (struct task_struct *)kalloc();
        task[i] -> state = TASK_RUNNING;
        task[i] -> counter = 0;
        task[i] -> pid = i;
        //task[i] -> priority = PRIORITY_MAX - rand() % (PRIORITY_MAX - PRIORITY_MIN + 1);
        task[i] -> priority = PRIORITY_MIN + rand() % (PRIORITY_MAX - PRIORITY_MIN + 1);
        task[i] -> thread.ra = (uint64_t)__dummy;
        task[i] -> thread.sp = PGSIZE + (uint64_t)task[i];  //内核态栈
        task[i] -> pgd = alloc_page();
        memcpy(task[i]->pgd , swapper_pg_dir , PGSIZE);
        do_mmap(&task[i]->mm,USER_END-PGSIZE,PGSIZE,0,0,VM_READ | VM_WRITE | VM_ANON);
        //task[i]->files = file_init();
        task[i] -> thread.sepc = USER_START;//将 sepc 设置为 USER_START
        uint64_t cur_sstatus = csr_read(sstatus);
        cur_sstatus &= 0xFFFFFFFFFFFFFFEF;//set SPP
        cur_sstatus |= 0x0000000000040020;//set SPIE,SUM
        task[i] -> thread.sstatus = cur_sstatus;//配置 sstatus 中的 SPP（使得 sret 返回至 U-Mode）、SPIE（sret 之后开启中断）、SUM（S-Mode 可以访问 User 页面）
        task[i] -> thread.sscratch = USER_END;//将 sscratch 设置为 U-Mode 的 sp，其值为 USER_END （将用户态栈放置在 user space 的最后一个页面）      
        nr_tasks++;
        load_program(task[i]);
        
        printk("[S-MODE] TASK1 INITIALIZED\n");
        //uint64_t user_sp = alloc_page();
        //uint64_t user_va = USER_END - PGSIZE;
        //uint64_t user_pa = (uint64_t)user_sp - (uint64_t)PA2VA_OFFSET;
        //create_mapping(task[i] -> pgd ,  user_va , user_pa , PGSIZE , 23);
        
    }
    for(int i=2;i<NR_TASKS;i++){
        task[i]=NULL;
    }
    printk("...task_init done!\n");
}

#if TEST_SCHED
#define MAX_OUTPUT ((NR_TASKS - 1) * 10)
char tasks_output[MAX_OUTPUT];
int tasks_output_index = 0;
char expected_output[] = "2222222222111111133334222222222211111113";
#include "sbi.h"
#endif

void dummy() {
    //printk("enter dummy\n");
    uint64_t MOD = 1000000007;
    uint64_t auto_inc_local_var = 0;
    int last_counter = -1;
    while (1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if (current->counter == 1) {
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
            #if TEST_SCHED
            tasks_output[tasks_output_index++] = current->pid + '0';
            if (tasks_output_index == MAX_OUTPUT) {
                for (int i = 0; i < MAX_OUTPUT; ++i) {
                    if (tasks_output[i] != expected_output[i]) {
                        printk("\033[31mTest failed!\033[0m\n");
                        printk("\033[31m    Expected: %s\033[0m\n", expected_output);
                        printk("\033[31m    Got:      %s\033[0m\n", tasks_output);
                        sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
                    }
                }
                printk("\033[32mTest passed!\033[0m\n");
                printk("\033[32m    Output: %s\033[0m\n", expected_output);
                sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
            }
            #endif
        }
    }
}


void switch_to(struct task_struct *next) {
    // YOUR CODE HERE
    //printk("switch_to: switch from [PID = %d] to [PID = %d]\n", current->pid, next->pid);
    struct task_struct *new_task = NULL;
    if(current != next){
        new_task = current ;
        current = next;
        __switch_to(new_task, current);
    }
}

void do_timer() {
    // 1. 如果当前线程是 idle 线程或当前线程时间片耗尽则直接进行调度
    // 2. 否则对当前线程的运行剩余时间减 1，若剩余时间仍然大于 0 则直接返回，否则进行调度
    //printk("enter do_timer\n");
    if(current -> pid == 0){
        schedule();//如果当前线程是 idle 线程
    }else if(current -> counter == 0){
        schedule();//当前线程时间片耗尽则直接进行调度
    }else{
        current->counter--;

        if(current->counter>0){
            return;
        }else{
            schedule();
        }
    }
    // YOUR CODE HERE
}

void schedule() {
    //printk(" enter schedule\n");
    int curmax = 0;
    int next = 0;
    
    // 遍历所有任务，选择 counter 最大的 TASK_RUNNING 状态的任务
    for (int i = 1; i < NR_TASKS; i++) {
        if (task[i] == NULL) continue;

        if (task[i]->state == TASK_RUNNING && task[i]->counter > curmax) {
            next = i;
            curmax = task[i]->counter;
        }
    }

    // 如果没有有效的任务（所有 counter == 0），则重置 counters 并重新调度
    if (curmax == 0) {
        printk("\n");
        for (int i = 1; i < NR_TASKS; i++) {
            if (task[i]) {
                task[i]->counter = task[i]->priority;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
            }
        }
        schedule(); // 重新调用调度
    } else {
        // 切换到选择的任务
        printk("\nswitch to [PID = %d PRIORITY = %d COUNTER = %d]\n", task[next]->pid, task[next]->priority, task[next]->counter);
        switch_to(task[next]);
    }
}


