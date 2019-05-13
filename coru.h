/*
 * coro, a small coroutine library
 */
#ifndef CORU_H
#define CORU_H

// TODO open questions
// - Should callbacks be able to return errors?
// - Is EAGAIN/0 appropriate for coru_resume?
// - Can coru_yield return errors?
// - What to do if we thread_resume(ourselves)?

#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"

// Possible error codes, these are negative to allow
// valid positive return values
enum coru_error {
    CORU_ERR_OK     = 0,    // No error
    CORU_ERR_AGAIN  = -11,  // Try again
    CORU_ERR_NOMEM  = -12,  // Out of memory
    CORU_ERR_INVAL  = -22,  // Invalid parameter
};

typedef struct coru {
    // stack information
    void *sp;

    // buffer if allocated
    void *allocated;
} coru_t;


// Create a coroutine, dynamically allocating memory for the stack
int coru_create(coru_t *coru, void (*cb)(void*), void *data, size_t size);

// Create a coroutine using the provided buffer to store the stack
int coru_create_inplace(coru_t *coru,
        void (*cb)(void*), void *data,
        void *buffer, size_t size);

// Cleans up any resources dedicated to the coroutine
void coru_destroy(coru_t *coru);

// Resume a coroutine
//
// This either proceeds from the last call to yield, or starts the coroutine if
// it has not been started yet
//
// Returns CORU_ERR_EAGAIN if the coroutine does not complete during this call.
// If the coroutine completes during this call, or had already completed, 0 is
// returned.
int coru_resume(coru_t *coru);

// Yields from inside a running coroutine.
void coru_yield(void);

#endif
