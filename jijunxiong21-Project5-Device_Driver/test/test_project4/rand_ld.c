#include <time.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_MEM_TEST_NUM 50
#define PAGE_TEST_NUM 10
#define PAGE_SIZE 0x1000

int value_test(long *mem_ptr, long mem_val, int count, int print_location) {
    // sys_move_cursor(0, print_location);
    // printf("> [TASK] to load: 0x%08lx <- %08ld, count: %d      ", mem_ptr, mem_val, count);

    // write the value into the address
    *mem_ptr = mem_val;

    // sys_move_cursor(0, print_location + 1);
    // printf("> [TASK] finish: 0x%08lx <- %08ld, count: %d      ", mem_ptr, mem_val, count);

    // read the value from the address
    if (*mem_ptr != mem_val) {
        sys_move_cursor(0, print_location + 2);
        printf("> [ERROR] Value generated are not matched with load result!");
        assert(0)
    }
    return 1;
}

int main(int argc, char* argv[]) {
    assert(argc > 0);
    int print_location = (argc > 1) ? atoi(argv[1]) : 0;

    /* pointer, reserved for random memory address
     * range: 0x1000_0000 - 0xefff_ffff
     */
    long *mem_ptr[MAX_MEM_TEST_NUM], *mem_ptr_base;
    long mem_val[MAX_MEM_TEST_NUM];
    clock_t start, end;

    // set a random seed
    srand(clock());

    // get a random memory address base
    mem_ptr_base = (long *)((rand() % 0xd0000000ul + 0x10000000ul) & ~(4ul - 1ul)); // 4-byte aligned
    // generate random memory address and value
    for (int i = 0; i < MAX_MEM_TEST_NUM; ++i) {
        mem_ptr[i] = mem_ptr_base + i * PAGE_SIZE;
        mem_val[i] = rand(); // get a random value to the memory address
    }

    sys_move_cursor(0, print_location);
    printf("> [TASK] Randomrized, pointers and values are generated.");

    // test full page memory swapping speed
    start = clock();
    for (int i = 0; i < 10000; i++) {
        for (int mem_idx = 0; mem_idx < MAX_MEM_TEST_NUM; ++mem_idx) {
            assert(value_test(mem_ptr[mem_idx], mem_val[mem_idx], i, print_location + 1) == 1)
        }
    }
    end = clock();
    sys_move_cursor(0, print_location + 3);
    printf("> [TASK] This is a thread to test random loading! (%d ticks).\n",
           (end - start) / MAX_MEM_TEST_NUM);

    // test no-swap speed
    start = clock();
    for (int i = 0; i < 10000; i++) {
        for (int mem_idx = 0; mem_idx < PAGE_TEST_NUM; ++mem_idx) {
            assert(value_test(mem_ptr[mem_idx], mem_val[mem_idx], i, print_location + 4) == 1)
        }
    }
    end = clock();
    sys_move_cursor(0, print_location + 6);
    printf("> [TASK] This is a thread to test random loading! (%d ticks).\n",
           (end - start) / PAGE_TEST_NUM);

    sys_move_cursor(0, print_location + 7);
    printf("Success!");
    return 0;
}

/*
loadbootm
exec rand_ld &
*/
