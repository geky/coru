#!/bin/bash
set -eu

echo "=== Parallel coru tests ==="

parallel_test() {
echo "--- Parallel test, $1 coroutines ---"
tests/test.py << TEST
    coru_t coru[$1];

    void count(void *p) {
        int j = (intptr_t)p;
        for (int i = 0; i < 10; i++) {
            printf("coru: %d, count: %d\n", j, i);
            coru_yield();
        }
    }

    void test() {
        for (int j = 0; j < $1; j++) {
            coru_create(&coru[j], count, (void*)(intptr_t)j, 8192) => 0;
        }

        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < $1; j++) {
                coru_resume(&coru[j]) => CORU_ERR_AGAIN;
                test_expect("coru: %d, count: %d\n", j, i);
            }
        }

        for (int j = 0; j < $1; j++) {
            coru_resume(&coru[j]) => 0;
            coru_destroy(&coru[j]);
        }
    }
TEST
}

parallel_test 2
parallel_test 3
parallel_test 10
parallel_test 100
