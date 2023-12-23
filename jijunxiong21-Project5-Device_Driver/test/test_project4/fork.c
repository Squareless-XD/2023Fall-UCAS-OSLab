#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

pid_t fork_test(void)
{
    return sys_fork();
}

int main(int argc, char *argv[])
{
    assert(argc > 0);
    int print_location = (argc > 1) ? atoi(argv[1]) : 0;

    sys_move_cursor(0, print_location);
    printf("try to fork a new process!");

    int parent_posi = 0;
    int count = 0;

    // call a child function, which will return pid or 0
    for (int i = 0; i < 10; i++)
    {
        pid_t pid = fork_test();
        if (pid == 0)
        {
            count++;
            sys_move_cursor(0, print_location);
            printf("child process is running! count: %d             ", sys_getpid());
        }
        else
        {
            parent_posi = i + 1;
            sys_move_cursor(0, print_location + parent_posi);
            printf("parent process is running! new pid: %d         ", pid);
            sys_waitpid(pid);
            break;
        }
    }
    sys_move_cursor(0, print_location + 11);
    printf("process exit! count: %d         ", count);

    return 0;
}
/* 
loadbootm
exec fork &
*/