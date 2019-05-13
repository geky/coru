/*
 * coru, a small coroutine library
 */
#include "coru.h"
#include "assert.h"

// Global object for the currently running coroutine
//
// Note that the active coroutine's stack pointer is swapped with
// its parent's while running
static coru_t *coru_active = NULL;

//// Platform specific functions, implemented below ////

// Initialize a coroutine stack
//
// This should set up the stack so that two things happen:
// 1. On the first call to coru_plat_yield, the callback cb should be called
//    with the data argument.
// 2. After the callback cb returns, the coroutine should then transfer control
//    to coru_halt, which does not return.
//
// After coru_plat_init, sp should contain the stack pointer for the new
// coroutine. Also, canary can be set to the end of the stack to enable best
// effort stack checking. Highly suggested.
//
// Any other platform initializations or assertions can be carried out here.
int coru_plat_init(void **sp, uintptr_t **canary,
        void (*cb)(void*), void *data,
        void *buffer, size_t size);

// Yield a coroutine
//
// This is where the magic happens.
//
// Several things must happen:
// 1. Store any callee saved registers/state
// 2. Store arg in temporary register
// 3. Swap sp and stack pointer, must store old stack pointer in sp
// 4. Return arg from temporary register
//
// Looking at the i386 implementation may be helpful
uintptr_t coru_plat_yield(void **sp, uintptr_t arg);


//// Coroutine operations ////

int coru_create(coru_t *coru, void (*cb)(void*), void *data, size_t size) {
    void *buffer = malloc(size);
    if (!buffer) {
        return CORU_ERR_NOMEM;
    }

    int err = coru_create_inplace(coru, cb, data, buffer, size);
    coru->allocated = buffer;
    return err;
}

int coru_create_inplace(coru_t *coru,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    coru->canary = NULL;
    coru->allocated = NULL;

    int err = coru_plat_init(&coru->sp, &coru->canary, cb, data, buffer, size);
    if (err) {
        return err;
    }

    if (coru->canary) {
        *coru->canary = (uintptr_t)0x636f7275;
    }

    return 0;
}

void coru_destroy(coru_t *coru) {
    free(coru->allocated);
}

int coru_resume(coru_t *coru) {
    // push previous coroutine's info on the current stack
    coru_t *prev = coru_active;
    coru_active = coru;
    // yield into coroutine
    int state = coru_plat_yield(&coru->sp, 0);
    // restore previous coroutine's info
    coru_active = prev;
    return state;
}

void coru_yield(void) {
    // do nothing if we are not a coroutine, this lets yield be used in
    // shared libraries
    if (!coru_active) {
        return;
    }

    // check canary, if this fails a stack overflow occured
    assert(!coru_active->canary ||
            *coru_active->canary == (uintptr_t)0x636f7275);

    // yield out of coroutine
    coru_plat_yield(&coru_active->sp, CORU_ERR_AGAIN);
}

// terminate a coroutine, not public but must be called below
// when a coroutine ends
void coru_halt(void) {
    while (true) {
        coru_plat_yield(&coru_active->sp, 0);
    }
}


//// Platform specific operations ////

#if defined(__i386__)
// Setup stack
int coru_plat_init(void **psp, uintptr_t **pcanary,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    // check that stack is aligned
    // TODO do this different? match equeue?
    assert((uint32_t)buffer % 4 == 0 && size % 4 == 0);
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
    assert((uint64_t)buffer % 4 == 0 && size % 4 == 0);
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
    assert((uint32_t)buffer % 4 == 0 && size % 4 == 0);
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
#else
#error "Unknown platform! Please update coru.h and coru.c"
#endif
