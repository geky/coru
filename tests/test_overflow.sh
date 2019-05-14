#!/bin/bash
set -eu

echo "=== Stack overflow tests ==="

echo "--- Stack overflow test ---"
tests/test.py -e << TEST
    coru_t coru;

    void recurse(void *p) {
        coru_yield();
        recurse(p);
        coru_yield();
    }

    void test() {
        coru_create(&coru, recurse, NULL, 8192) => 0;

        for (int i = 0; i < 1024; i++) {
            coru_resume(&coru) => CORU_ERR_AGAIN;
        }
    }
TEST
