/*
 * coru, a small coroutine library
 */
#include "coru.h"
#include "assert.h"

// Global object for the currently running coroutine
//
// Note that the active coroutine's stack pointer is swapped with
// its parent's while running
static coru_t *coru_parent;

//// Platform specific functions, implemented below ////

// Initialize a coroutine stack
//
// This should set up the stack so that two things happen:
// 1. On the first call to coru_plat_yield, the callback cb should be called
//    with the data argument.
// 2. After the callback cb returns, the coroutine should then transfer control
//    to coru_halt, which does not return.
//
// Any other platform initializations or assertions can be carried out here.
int coru_plat_init(void **sp,
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
void *coru_plat_yield(void **sp, void *arg);


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
    coru->allocated = NULL;
    return coru_plat_init(&coru->sp, cb, data, buffer, size);
}

void coru_destroy(coru_t *coru) {
    free(coru->allocated);
}

int coru_resume(coru_t *coru) {
    coru_t *prev = coru_parent;
    coru_parent = coru;
    int state = (int)coru_plat_yield(&coru->sp, (void*)0);
    coru_parent = prev;
    return state;
}

void coru_yield(void) {
    coru_plat_yield(&coru_parent->sp, (void*)CORU_ERR_AGAIN);
}

// terminate a coroutine, not public but must be called below
// when a coroutine ends
void coru_halt(void) {
    while (true) {
        coru_plat_yield(&coru_parent->sp, (void*)0);
    }
}


//// Platform specific operations ////

#ifdef __i386__
// Setup stack
int coru_plat_init(void **psp,
        void (*cb)(void*), void *data,
        void *buffer, size_t size) {
    // TODO do this different? match equeue?
    assert((uint32_t)buffer % 4 == 0 && (uint32_t)size % 4 == 0);
    assert(size > 8*sizeof(uint32_t));
    uint32_t *sp = (uint32_t*)((char*)buffer + size);

    // setup stack
    sp[-7] = 0;                     // edi
    sp[-6] = 0;                     // esi
    sp[-5] = 0;                     // ebx
    sp[-4] = 0;                     // ebp (frame pointer)
    sp[-3] = (uint32_t)cb;          // ret to cb(data)
    sp[-2] = (uint32_t)coru_halt;   // ret to coru_halt()
    sp[-1] = (uint32_t)data;        // arg to cb(data)
    *psp = &sp[-7];
    return 0;
}

// Swap stacks
extern void *coru_plat_yield(void **sp, void *arg);
__asm__ (
    ".globl coru_plat_yield \n"
    "coru_plat_yield: \n"
    "\t mov 8(%esp), %eax \n"
    "\t mov 4(%esp), %edx \n"
    "\t push %ebp \n"
    "\t push %ebx \n"
    "\t push %esi \n"
    "\t push %edi \n"
    "\t xchg %esp, (%edx) \n"
    "\t pop %edi \n"
    "\t pop %esi \n"
    "\t pop %ebx \n"
    "\t pop %ebp \n"
    "\t ret \n"
);
#else
#error "Unknown platform! Please update coru.h and coru.c"
#endif
