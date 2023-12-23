#include <stdio.h>
#include <unistd.h>

int64_t counter1 = 0;
int64_t counter2 = 0;
int64_t yield_cnt1 = 0;
int64_t yield_cnt2 = 0;

void thread_count0()
{
    int print_location = 6;
    while (1)
    {
        counter1++;
        sys_move_cursor(0, print_location);
        printf("> [TASK] Thread 1 counter: %d.", counter1);
        if (counter1 - counter2 >= 20)
        {
            yield_cnt1++;
            sys_thread_yield();
        }
    }
}

void thread_count1()
{
    int print_location = 7;
    while (1)
    {
        counter2++;
        sys_move_cursor(0, print_location);
        printf("> [TASK] Thread 2 counter: %d.", counter2);
        if (counter2 - counter1 >= 20)
        {
            yield_cnt2++;
            sys_thread_yield();
        }
    }
}

int main(void)
{
    int print_location = 8;

    sys_move_cursor(0, 6);
    printf("> [TASK] Thread 1 counter: %d.", counter1);
    sys_move_cursor(0, 7);
    printf("> [TASK] Thread 2 counter: %d.", counter2);

    sys_thread_create(thread_count0, 0);
    sys_thread_create(thread_count1, 0);

    while (1)
    {
        // test whether the thread counted too many times
        sys_move_cursor(0, print_location);
        printf("> [TASK] Thread yield counter: %d v.s. %d.", yield_cnt1, yield_cnt2);
        sys_thread_yield();
    }
}
