#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define BUF_LEN 10

/*
    该测试首先启动 fly 小飞机程序并将其绑定到主核上，保证测
    试过程中 fly 小飞机程序的运行，随后分别在主核和从核上运行 fine_grained_lock
    测试，在屏幕上输出 fine_grained_lock 运行的 ticks 数。
*/

int main(int argc, char *argv[])
{
    assert(argc >= 1);
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);

    // print start message
    sys_move_cursor(0, print_location);
    printf("> [TASK] This is final test.\n");

    // start fly by exec
    char buf_location_fly[BUF_LEN];
    assert(itoa(print_location + 1, buf_location_fly, BUF_LEN, 10) != -1);

    char *argv_fly[2] = {"fly", buf_location_fly};
    pid_t fly_pid = sys_exec(argv_fly[0], 2, argv_fly);
    assert(fly_pid != 0);

    // bind fly to core 0
    int taskset_ret = sys_taskset(0x1, fly_pid, NULL, 0, NULL);
    assert(taskset_ret == 0);

    // start fine_grained_lock by exec
    for (int i = 0; i < 2; i++)
    {
        char buf_location_fgl[BUF_LEN];
        assert(itoa(print_location + 1 + i, buf_location_fgl, BUF_LEN, 10) != -1);
        char *argv_fgl[2] = {"fine_grained_lock", buf_location_fgl};

        pid_t fgl_pid = sys_exec(argv_fgl[0], 2, argv_fgl);
        assert(fgl_pid != 0);

        // bind  fine_grained_lock to core 0
        taskset_ret = sys_taskset(i + 1, fgl_pid, NULL, 0, NULL);
        assert(taskset_ret == 0);

        // wait for fine_grained_lock to exit
        sys_waitpid(fgl_pid);
    }

    // kill fly
    sys_kill(fly_pid);

    return 0;
}