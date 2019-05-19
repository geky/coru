#!/bin/bash
set -eu

echo "=== Nested coru tests ==="

nested_test() {
echo "--- Nested test, $1 coroutines ---"
tests/test.py << TEST
    coru_t coru[$1];

    void count(void *p) {
        int j = (intptr_t)p;
        if (j > 0) {
            coru_create(&coru[j-1], count, (void*)(intptr_t)(j-1), 8192) => 0;
        }

        for (int i = 0; i < 10; i++) {
            if (j > 0) {
                coru_resume(&coru[j-1]) => CORU_ERR_AGAIN;
                test_expect("coru: %d, count: %d\n", j-1, i);
            }

            printf("coru: %d, count: %d\n", j, i);
            coru_yield();
        }

        if (j > 0) {
            coru_resume(&coru[j-1]) => 0;
            coru_destroy(&coru[j-1]);
        }
    }

    void test(void) {
        coru_create(&coru[$1-1], count, (void*)(intptr_t)($1-1), 8192) => 0;

        for (int i = 0; i < 10; i++) {
            coru_resume(&coru[$1-1]) => CORU_ERR_AGAIN;
            test_expect("coru: %d, count: %d\n", $1-1, i);
        }

        coru_resume(&coru[$1-1]) => 0;
        coru_destroy(&coru[$1-1]);
    }
TEST
}

nested_test 2
nested_test 3
nested_test 10
nested_test 100
