#include "coru.h"
#include "stdio.h"

#include "assert.h"

coru_t test_coru;

void test_count(void *p) {
    (void)p;
    for (int i = 0; i < 10; i++) {
        printf("test_count %d\n", i);
        coru_yield();
    }
}

int main() {
    int err = coru_create(&test_coru, test_count, (void*)0x11223344, 512*1024);
    assert(!err);

    for (int i = 0; i < 15; i++) {
        err = coru_resume(&test_coru);
        printf("main %d\n", err);
    }

    return 0;
}
