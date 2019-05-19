## coru

A pocket coroutine library.

coru was built to solve a common problem for MCU development: Integrating
blocking code with an event-driven system.

When this happens, MCU developers usually turn to an RTOS. But this is often
like using a sledgehammer to swat a fly. An RTOS introduces complexity, owns
the execution environment, and while an RTOS does provide a rich set of
scheduling features, it comes with an equally large code cost.

coru provides a much simpler solution with only a 300 B code cost, half the
size of this introduction.

### How to use coru

coru provides delimited coroutines, which are sort of like threads but without
preemption.

You give coru a function and a stack size and it creates a coroutine:

``` c
void func(void *) {
    printf("hi!\n");
}


coru_t co;
coru_create(&co, func, NULL, 4096); // returns 0
```

You can start the coroutine by calling coru_resume:

``` c
coru_resume(&co); // returns 0, prints hi!
```

In this case, our function will print "hi!" and coru_resume will block until
the function exits.

But things get really interesting when we call coru_yield in our coroutine:

``` c
void func(void *) {
    for (int i = 0; i < 10; i++) {
        printf("hi %d!\n", i);
        coru_yield();
    }
}


coru_t co;
coru_create(&co, func, NULL, 4096); // returns 0
```

Now, when we call coru_resume, our function will run until the first
coru_yield:

``` c
coru_resume(&co); // returns CORU_ERR_AGAIN, prints hi 0!
```

At this point, our function has been paused to give the main thread a chance to
run. We can resume it with, you guessed it, coru_resume:

``` c
coru_resume(&co); // returns CORU_ERR_AGAIN, prints hi 1!
coru_resume(&co); // returns CORU_ERR_AGAIN, prints hi 2!
coru_resume(&co); // returns CORU_ERR_AGAIN, prints hi 3!
...
coru_resume(&co); // returns CORU_ERR_AGAIN, prints hi 9!
coru_resume(&co); // returns 0
```

coru_resume returns CORU_ERR_AGAIN while the function is running, and returns 0
once the function finishes. We can still call coru_resume, but it will return 0
and do nothing.

``` c
coru_resume(&co); // returns 0
coru_resume(&co); // returns 0
coru_resume(&co); // returns 0
```

When you're done with the coroutine, don't forget to clean up its resources
with coru_destroy:

``` c
coru_destroy(&co);
```

### No malloc? No worries

By default, coru will try to use malloc to create the stack for each coroutine.
You can avoid this by either redefining CORU_MALLOC/CORU_FREE in coru_utils.h
or by calling coru_create_inplace:

``` c
coru_t co;
uint8_t co_stack[4096];

coru_create_inplace(&co, func, NULL, co_stack, 4096); // returns 0
```

### What if I overflow my stack?

coru does provide a simple stack canary, which _usually_ catches stack
overflows. If a stack overflow is detected, coru asserts. At this point the
program can't continue as who knows what memory has been corrupted.

``` c
void func(void *) {
    func(NULL);
    printf("hi!");
}

coru_t co;
coru_create(&co, func, NULL, 512); // returns 0
coru_resume(&co); // assertion fails, stack overflow
```

Unfortunately, knowing how much stack to allocate is a hard problem.

### Where are the mutexes?

There's no race conditions here! No preemption has a big benefit in that state
can only change during coru_resume or coru_yield, a granularity that's much
easier for us humans to reason about. Mutate away!

``` c
int counter = 0;

void func(void *) {
    while (true) {
        // increment the global counter
        counter = counter + 1;
        printf("%d\n", i);
    }
}

coru_t co1;
coru_create(&co1, func, NULL, 4096); // returns 0
coru_t co2;
coru_create(&co2, func, NULL, 4096); // returns 0

coru_resume(&co1); // returns CORU_ERR_AGAIN, prints 1
coru_resume(&co2); // returns CORU_ERR_AGAIN, prints 2
coru_resume(&co1); // returns CORU_ERR_AGAIN, prints 3
coru_resume(&co2); // returns CORU_ERR_AGAIN, prints 4
...
```

Ok, I take that back. Mutate responsibly. Even with coroutines, a large amount
of mutable global state can lead to confusing and unmaintainable programs.

### Where's the scheduling?

So, a part of keeping coru small is that it doesn't have a scheduler. Sometimes
you don't need one or have your own.

If you do need a scheduler, coru is intended to work well with
[equeue](https://github.com/geky/equeue), its sister event queue library.

Here's an example of running a coroutine in the background of an event queue
using equeue. If you're not using equeue you should still be able to use this
technique with your own scheduler.

``` c
// waiting logic
equeue_t q;
int task_next_wait = 0;

void task_run(void *p) {
    coru_t *co = p;

    next_wait = 0;
    int err = coru_resume(co);
    if (err == CORU_ERR_AGAIN) {
        equeue_call_in(&q, task_next_wait, task_run, co); // returns id
    }
}

void task_wait(int ms) {
    task_next_wait = ms;
    coru_yield();
}

// our task
void func(void *) {
    while (true) {
        printf("waiting 1000 ms...\n");
        task_wait(1000); // wait 1000 ms
    }
}

// create task and event queue
coru_t co;
coru_create(&co, func, NULL, 4096); // returns 0

equeue_create(&q, 4096); // returns 0
equeue_call(&q, task_run, &co); // returns id
equeue_dispatch(&q, -1); // runs q, prints "waiting 1000 ms..." every 1000 ms
```

### But my libraries!

Right, so a big concern with coroutine systems is how to handle third-party
libraries. The problem is that you can't control when a library calls yield
and need preemption to force libraries to give up the CPU.

But really, in most cases, the only code that takes any real amount of time
is in drivers, code that waits on hardware.

Consider [littlefs](https://github.com/geky/littlefs). littlefs must be ported
to a platform's block device, so we already have to write some code. Maybe we
have to poll a flag in our block device. With coru we can turn any polling into
polite yielding.

``` c
int spif_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    uint32_t addr = block*cfg->block_size + off;
    uint8_t cmd[4] = {
        SPIF_READ,
        0xff & (addr >> 16),
        0xff & (addr >> 8),
        0xff & (addr >> 0)
    };

    // send read command
    int err = hal_spi_transfer(cmd, 4, NULL, 0);
    if (err) {
        return err;
    }

    // wait for DMA
    while (!hal_spi_isdone()) {
        coru_yield(); // yield while polling
    }

    // read block
    int err = hal_spi_transfer(NULL, 0, buffer, size);
    if (err) {
        return err;
    }

    // wait for DMA
    while (!hal_spi_isdone()) {
        coru_yield(); // yield while polling
    }
}

// repeat for spif_prog, spif_erase, spif_sync...
```

We can then even wrap up our filesystem operations in coru_resume:

``` c
struct asyncfile {
    coru_t *co;
    int res;

    lfs_t *lfs;
    lfs_file_t *file;
    void *buffer;
    lfs_size_t size;
};

void asyncfile_run(void *p) {
    // coroutine for non-blocking read
    struct asyncfile *af = p;
    af->res = lfs_file_read(af->lfs, af->file, af->buffer, af->size);
}

int asyncfile_open(struct asyncfile *af,
        lfs_t *lfs, const char *path, int flags) { 
    int err = lfs_file_open(lfs, &af->file, path, flags);
    if (err) {
        return err;
    }

    err = coru_create(&af->co, asyncfile_run, af, 4096);
    if (err) {
        return err;
    }

    af->buffer = NULL;
    af->size = 0;
    return 0;
}

int asyncfile_read(struct asyncfile *af,
        void *buffer, lfs_size_t size) {
    // only setup buffer on first read that would block
    if (!af->buffer) {
        af->res = 0;
        af->buffer = buffer;
        af->size = size;
    }

    // step coroutine, returns CORU_ERR_AGAIN if would block
    int err = coru_resume(&af->co);
    if (err) {
        return err;
    }

    // completed a read, reset for next read
    af->buffer = NULL;
    af->size = 0;
    return af->res;
}

int asyncfile_close(struct asyncfile *af) {
    coru_destroy(&af->co);

    int err = lfs_file_close(af->lfs, af->file);
    if (err) {
        return err;
    }

    return 0;
}
```

And hey, now littlefs is non-blocking. That's cool. Sure littlefs may take many
block device operations to read a file, but each operation is a slice where we
give other tasks a chance to run.

More realistically, you would move the coroutine handling up higher into your
application, with a handful of hardware specific processes that each run in
their own coroutines.

This is the most common case for MCU libraries. There are a few exceptions, for
example a software crypto library, but for these special cases it's not
unreasonable to manually inject coru_yield calls.

``` c
int cmac(uint8_t *output, const uint8_t *input, size_t input_size) {
    // create cipher
    int err;
    mbedtls_cipher_context_t ctx;
    mbedtls_cipher_init(ctx);
    const mbedtls_cipher_info_t *cipher_info =
            mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
    err = mbedtls_cipher_setup(ctx, cipher_info);
    if (err) {
        goto cleanup;
    }
    err = mbedtls_cipher_cmac_starts(ctx, auth_key, AUTH_KEY_SIZE_BITS);
    if (err) {
        goto cleanup;
    }

    // calculate cmac
    for (int i = 0; i < input_size; i += chunk_size) {
        err = mbedtls_cipher_cmac_update(ctx, &input[i], CMAC_CHUNK_SIZE);
        if (err) {
            goto cleanup;
        }

        coru_yield(); // yield in loop that consumes CPU
    }

    // clean up resources
    err = mbedtls_cipher_cmac_finish(ctx, output);
    if (err) {
        goto cleanup;
    }
cleanup:
    mbedtls_cipher_free(ctx);
    return err;
}
```

To help with this, coru_yield outside of a coroutine is simply a noop.

### Tests?

Run make test:

``` bash
make test
```

If [QEMU](https://www.qemu.org) supports your processor, you can even
cross-compile these tests:

``` bash
make test CC="arm-linux-gnueabi-gcc --static -mthumb" EXEC="qemu-arm"
```

### What if my processor isn't supported?

Fret not! The only reason this project exists is to make it easy to port
coroutines to new platforms. I tried to find another coroutine library that
did this, but all of the ones I found required quite a bit of effort to reverse
engineer the porting layer.

Coroutines _require_ instruction-set specific code in order to manipulate
stacks. This is the main reason coroutines have seen very little use in the MCU
space, where instruction sets can change from project to project. coru is
trying to change that.

coru requires only two functions:

``` c
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
```

It may be helpful to look at the other implementation in
[coru_platform.c](coru_platform.c) to see how these can be implemented.

And if you port coru to a new platform, please create a PR! It would be amazing
to see coru become the biggest collection of MCU stack manipulation functions.

### But geky, my processor is a novel quinary vector machine whos only branch instruction is return-if-prime!

You have more problems than I can help you with.

### Related projects

- [equeue](https://github.com/geky/equeue) - A Swiss Army knife scheduler for
  MCUs. equeue is the sister library to coru and provides a simple event-based
  scheduler. These two libraries can provide a solid foundation to systems
  where a full RTOS may be unnecessary.

- [Lua coroutines](https://www.lua.org/pil/9.1.html) - coru is based heavily on
  Lua's coroutine library, which was a big inspiration for this library and is
  one of the best introductions to coroutines in general.
