/*
 * coru, platform specific functions
 *
 * Copyright (c) 2019 Christopher Haster
 * Distributed under the MIT license
 */
#include "coru.h"
#include "coru_platform.h"


// Terminate a coroutine, defined in coru.c
//
// Must be called when coroutine's main function returns.
extern void coru_halt(void);


// Platform specific operations

// x86 32-bits
#if defined(__i386__)

// Setup stack
int coru_plat_init(void **psp, uintptr_t **pcanary,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    // check that stack is aligned
    // TODO do this different? match equeue?
    CORU_ASSERT((uint32_t)buffer % 4 == 0 && size % 4 == 0);
    uint32_t *sp = (uint32_t*)((char*)buffer + size);

    // setup stack
    sp[-7] = 0;                     // edi
    sp[-6] = 0;                     // esi
    sp[-5] = 0;                     // ebx
    sp[-4] = 0;                     // ebp (frame pointer)
    sp[-3] = (uint32_t)cb;          // ret to cb(data)
    sp[-2] = (uint32_t)coru_halt;   // ret to coru_halt()
    sp[-1] = (uint32_t)data;        // arg to cb(data)

    // setup stack pointer and canary
    *psp = &sp[-7];
    *pcanary = &sp[-size/sizeof(uint32_t)];
    return 0;
}

// Swap stacks
extern uintptr_t coru_plat_yield(void **sp, uintptr_t arg);
__asm__ (
    ".globl coru_plat_yield \n"
    "coru_plat_yield: \n"
    "\t mov 8(%esp), %eax \n"   // save arg to eax, return this later
    "\t mov 4(%esp), %edx \n"   // load new esp to edx
    "\t push %ebp \n"           // push callee saved registers
    "\t push %ebx \n"
    "\t push %esi \n"
    "\t push %edi \n"
    "\t xchg %esp, (%edx) \n"   // swap stack
    "\t pop %edi \n"            // pop callee saved registers
    "\t pop %esi \n"
    "\t pop %ebx \n"
    "\t pop %ebp \n"
    "\t ret \n"                 // return eax
);

// x86 64-bits
#elif defined(__amd64__)

// Here we need a prologue to get data to the callback when
// we startup a coroutine.
extern void coru_plat_prologue(void);
__asm__ (
    ".globl coru_plat_prologue \n"
    "coru_plat_prologue: \n"
    "\t pop %rdi \n"
    "\t ret \n"
);

// Setup stack
int coru_plat_init(void **psp, uintptr_t **pcanary,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    // check that stack is aligned
    // TODO do this different? match equeue?
    CORU_ASSERT((uint64_t)buffer % 4 == 0 && size % 4 == 0);
    uint64_t *sp = (uint64_t*)((char*)buffer + size);

    // setup stack
    sp[-10] = 0;                            // r15
    sp[-9 ] = 0;                            // r14
    sp[-8 ] = 0;                            // r13
    sp[-7 ] = 0;                            // r12
    sp[-6 ] = 0;                            // rbx
    sp[-5 ] = 0;                            // rbp (frame pointer)
    sp[-4 ] = (uint64_t)coru_plat_prologue; // prologue to tie cb+data together
    sp[-3 ] = (uint64_t)data;               // arg to cb(data)
    sp[-2 ] = (uint64_t)cb;                 // ret to cb(data)
    sp[-1 ] = (uint64_t)coru_halt;          // ret to coru_halt()

    // setup stack pointer and canary
    *psp = &sp[-10];
    *pcanary = &sp[-size/sizeof(uint64_t)];
    return 0;
}

// Swap stacks
extern uintptr_t coru_plat_yield(void **sp, uintptr_t arg);
__asm__ (
    ".globl coru_plat_yield \n"
    "coru_plat_yield: \n"
    "\t push %rbp \n"           // push callee saved registers
    "\t push %rbx \n"
    "\t push %r12 \n"
    "\t push %r13 \n"
    "\t push %r14 \n"
    "\t push %r15 \n"
    "\t xchg %rsp, (%rdi) \n"   // swap stack
    "\t pop %r15 \n"            // pop callee saved registers
    "\t pop %r14 \n"
    "\t pop %r13 \n"
    "\t pop %r12 \n"
    "\t pop %rbx \n"
    "\t pop %rbp \n"
    "\t mov %rsi, %rax \n"      // return arg
    "\t ret \n"
);

// ARM thumb mode
#elif defined(__thumb__)

// Here we need a prologue to get both data and coru_halt
// into the appropriate registers.
extern void coru_plat_prologue(void);
__asm__ (
    ".thumb_func \n"
    ".global coru_plat_prologue \n"
    "coru_plat_prologue: \n"
    "\t pop {r0,r1,r2} \n"
    "\t mov lr, r2 \n"
    "\t bx r1 \n"
);

// Setup stack
int coru_plat_init(void **psp, uintptr_t **pcanary,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    // check that stack is aligned
    // TODO do this different? match equeue?
    CORU_ASSERT((uint32_t)buffer % 4 == 0 && size % 4 == 0);
    uint32_t *sp = (uint32_t*)((char*)buffer + size);

    // setup stack
    sp[-12] = 0;                            // r8
    sp[-11] = 0;                            // r9
    sp[-10] = 0;                            // r10
    sp[-9 ] = 0;                            // r11
    sp[-8 ] = 0;                            // r4
    sp[-7 ] = 0;                            // r5
    sp[-6 ] = 0;                            // r6
    sp[-5 ] = 0;                            // r7
    sp[-4 ] = (uint32_t)coru_plat_prologue; // prologue to tie things together
    sp[-3 ] = (uint32_t)data;               // arg to callback (r0)
    sp[-2 ] = (uint32_t)cb;                 // callback (r1)
    sp[-1 ] = (uint32_t)coru_halt;          // coru_halt (r2)

    // setup stack pointer and canary
    *psp = &sp[-12];
    *pcanary = &sp[-size/sizeof(uint32_t)];
    return 0;
}

// Swap stacks
extern uintptr_t coru_plat_yield(void **sp, uintptr_t arg);
__asm__ (
    ".thumb_func \n"
    ".global coru_plat_yield \n"
    "coru_plat_yield: \n"
    "\t push {r4,r5,r6,r7,lr} \n"   // push callee saved registers
    "\t mov r4, r8 \n"              // yes we need these moves, thumb1 can
    "\t mov r5, r9 \n"              // only push r0-r7 at the same time
    "\t mov r6, r10 \n"
    "\t mov r7, r11 \n"
    "\t push {r4,r5,r6,r7} \n"
    "\t mov r2, sp \n"              // swap stack, takes several instructions
    "\t ldr r3, [r0] \n"            // here because thumb1 can't load/store sp
    "\t str r2, [r0] \n"
    "\t mov sp, r3 \n"
    "\t mov r0, r1 \n"              // return arg
    "\t pop {r4,r5,r6,r7} \n"       // pop callee saved registers and return
    "\t mov r8, r4 \n"
    "\t mov r9, r5 \n"
    "\t mov r10, r6 \n"
    "\t mov r11, r7 \n"
    "\t pop {r4,r5,r6,r7,pc} \n"
);

// MIPS
#elif defined(__mips__)

// Here we need a prologue to get both data and coru_halt
// into the appropriate registers.
extern void coru_plat_prologue(void);
__asm__ (
    ".globl coru_plat_prologue \n"
    "coru_plat_prologue: \n"
    "\t move $ra, $s0 \n"       // setup $ra to return to core_halt()
    "\t addiu $sp, $sp, -4 \n"  // tail call cb(data)
    "\t move $a0, $s1 \n"
    "\t j $s2 \n"
);

// get $gp register which is used for position-independent code
extern uint32_t coru_plat_getgp(void);
__asm__ (
    ".globl coru_plat_getgp \n"
    "coru_plat_getgp: \n"
    "\t move $v0, $gp \n"
    "\t j $ra \n"
);

int coru_plat_init(void **psp, uintptr_t **pcanary,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    // check that stack is aligned
    // TODO do this different? match equeue?
    CORU_ASSERT((uint32_t)buffer % 4 == 0 && size % 4 == 0);
    uint32_t *sp = (uint32_t*)((char*)buffer + size);

    // setup stack
    sp[-11] = (uint32_t)coru_halt;          // $s0
    sp[-10] = (uint32_t)data;               // $s1
    sp[-9 ] = (uint32_t)cb;                 // $s2
    sp[-8 ] = 0;                            // $s3
    sp[-7 ] = 0;                            // $s4
    sp[-6 ] = 0;                            // $s5
    sp[-5 ] = 0;                            // $s6
    sp[-4 ] = 0;                            // $s7
    sp[-3 ] = coru_plat_getgp();            // $gp
    sp[-2 ] = 0;                            // $fp
    sp[-1 ] = (uint32_t)coru_plat_prologue; // $ra

    // setup stack pointer and canary
    *psp = &sp[-11];
    *pcanary = &sp[-size/sizeof(uint32_t)];
    return 0;
}

// Swap stacks
extern uintptr_t coru_plat_yield(void **sp, uintptr_t arg);
__asm__ (
    ".globl coru_plat_yield \n"
    "coru_plat_yield: \n"
    "\t addiu $sp, $sp, -44 \n" // push callee saved registers
    "\t sw $s0,  0($sp) \n"
    "\t sw $s1,  4($sp) \n"
    "\t sw $s2,  8($sp) \n"
    "\t sw $s3, 12($sp) \n"
    "\t sw $s4, 16($sp) \n"
    "\t sw $s5, 20($sp) \n"
    "\t sw $s6, 24($sp) \n"
    "\t sw $s7, 28($sp) \n"
    "\t sw $gp, 32($sp) \n"
    "\t sw $fp, 36($sp) \n"
    "\t sw $ra, 40($sp) \n"
    "\t lw $t0, ($a0) \n"       // swap stack
    "\t sw $sp, ($a0) \n"
    "\t move $sp, $t0 \n"
    "\t lw $s0,  0($sp) \n"     // pop callee saved registers
    "\t lw $s1,  4($sp) \n"
    "\t lw $s2,  8($sp) \n"
    "\t lw $s3, 12($sp) \n"
    "\t lw $s4, 16($sp) \n"
    "\t lw $s5, 20($sp) \n"
    "\t lw $s6, 24($sp) \n"
    "\t lw $s7, 28($sp) \n"
    "\t lw $gp, 32($sp) \n"
    "\t lw $fp, 36($sp) \n"
    "\t lw $ra, 40($sp) \n"
    "\t addiu $sp, $sp, 44 \n"
    "\t move $v0, $a1 \n"       // return arg
    "\t j $ra \n"
);

#else
#error "Unknown platform! Please update coru_platform.c"
#endif
