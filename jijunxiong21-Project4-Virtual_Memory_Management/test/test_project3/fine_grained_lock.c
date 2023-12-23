#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define LOCK_KEY 30

int main(int argc, char *argv[])
{
    assert(argc >= 1);
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);

    int handle = sys_mutex_init(LOCK_KEY); // get the mutex lock
    int start = clock(); // record the start ticks

    // acquire and release the lock for 10000 times
    for (int i = 0; i < 10000; i++)
    {
        sys_mutex_acquire(handle);
        sys_mutex_release(handle);
    }

    int end = clock(); // record the end ticks

    // print the result
    sys_move_cursor(0, print_location);
    printf("> [TASK] This is a thread to test mutex lock! (%d ticks).\n", end - start);

    return 0;
}
