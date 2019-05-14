#!/bin/bash
set -eu

echo "=== Simple tests ==="

echo "--- Single test ---"
tests/test.py << TEST
    coru_t coru;

    void count(void *p) {
        (void)p;
        for (int i = 0; i < 10; i++) {
            printf("count: %d\n", i);
            coru_yield();
        }
    }

    void test() {
        coru_create(&coru, count, NULL, 8192) => 0;

        for (int i = 0; i < 10; i++) {
            coru_resume(&coru) => CORU_ERR_AGAIN;
            test_expect("count: %d\n", i);
        }

        coru_resume(&coru) => 0;
        coru_destroy(&coru);
    }
TEST
