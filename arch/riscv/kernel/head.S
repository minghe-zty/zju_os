    .extern start_kernel
    .extern task_init
    .extern mm_init
    .section .text.init
    .globl _start
_start:
    la sp , boot_stack_top

    call setup_vm

    call relocate

    call mm_init

    call setup_vm_final

    call task_init
    
        
    # set stvec = _traps
    la      t0, _traps          
    csrw    stvec, t0           

    # set sie[STIE] = 1
    li      t1, (1 << 5)       
    csrr    t0, sie             
    or      t0, t0, t1          
    csrw    sie, t0             

    # set first time interrupt
    li      t1, 100000  
    csrr    t0, time                     
    add     t0, t0, t1          
    mv      a0, t0              
    li      a7, 0               
    ecall                      

    # set sstatus[SIE] = 1
    li      t1, (1 << 1)  
    csrr    t0, sstatus         
    or      t0, t0, t1          
    csrw    sstatus, t0    

    call start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t0, 0xffffffdf80000000 
    add ra, ra, t0
    add sp, sp, t0

    # need a fence to ensure the new translations are in use
    sfence.vma zero, zero

    # set satp with early_pgtbl
    li t1, 0x8000000000000000
    la t2, early_pgtbl
    srli t2, t2, 12   
    or t1, t2, t1
    csrw satp, t1
    ret

    .section .bss.stack
    .globl boot_stack
    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096                 # Adjust stack size as needed

    .globl boot_stack_top
boot_stack_top:
