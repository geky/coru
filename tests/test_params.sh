#!/bin/bash
set -eu

echo "=== Parameter tests ==="

echo "--- Simple create test ---"
tests/test.py << TEST
    coru_t coru;

    void count(void *p) {
        (void)p;
        printf("hey\n");
        coru_yield();
        printf("hi\n");
        coru_yield();
        printf("hello\n");
        coru_yield();
    }

    void test() {
        coru_create(&coru, count, NULL, 8192) => 0;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hey\n");
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hi\n");
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hello\n");
        coru_resume(&coru) => 0;
        coru_destroy(&coru);
    }
TEST

echo "--- Create with arg test ---"
tests/test.py << TEST
    coru_t coru;

    void count(void *p) {
        int *param = p;
        printf("hey %d\n", *param);
        coru_yield();
        printf("hi %d\n", *param);
        coru_yield();
        printf("hello %d\n", *param);
        coru_yield();
    }

    void test() {
        int param = 0;
        coru_create(&coru, count, &param, 8192) => 0;
        param = 1;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hey %d\n", param);
        param = 2;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hi %d\n", param);
        param = 3;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hello %d\n", param);
        coru_resume(&coru) => 0;
        coru_destroy(&coru);
    }
TEST

echo "--- Simple create inplace test ---"
tests/test.py << TEST
    coru_t coru;
    uint8_t buffer[8192];

    void count(void *p) {
        (void)p;
        printf("hey\n");
        coru_yield();
        printf("hi\n");
        coru_yield();
        printf("hello\n");
        coru_yield();
    }

    void test() {
        coru_create_inplace(&coru, count, NULL, buffer, 8192) => 0;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hey\n");
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hi\n");
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hello\n");
        coru_resume(&coru) => 0;
        coru_destroy(&coru);
    }
TEST

echo "--- Create inplace with arg test ---"
tests/test.py << TEST
    coru_t coru;
    uint8_t buffer[8192];

    void count(void *p) {
        int *param = p;
        printf("hey %d\n", *param);
        coru_yield();
        printf("hi %d\n", *param);
        coru_yield();
        printf("hello %d\n", *param);
        coru_yield();
    }

    void test() {
        int param = 0;
        coru_create_inplace(&coru, count, &param, buffer, 8192) => 0;
        param = 1;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hey %d\n", param);
        param = 2;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hi %d\n", param);
        param = 3;
        coru_resume(&coru) => CORU_ERR_AGAIN;
        test_expect("hello %d\n", param);
        coru_resume(&coru) => 0;
        coru_destroy(&coru);
    }
TEST
