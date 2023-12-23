#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static char blank[] = {"                                                                   "};
static char plane1[] = {"         _       "};
static char plane2[] = {"       -=\\`\\     "};
static char plane3[] = {"   |\\ ____\\_\\__  "};
static char plane4[] = {" -=\\c`\"\"\"\"\"\"\" \"`)"};
static char plane5[] = {"    `~~~~~/ /~~` "};
static char plane6[] = {"      -==/ /     "};
static char plane7[] = {"        '-'      "};

int main(int argc, char **argv)
{
    sys_move_cursor(0, 1);
    printf("Genshin impact: ...");

    assert(argc == 2);
    char *start_success = argv[1];

    sys_move_cursor(0, 2);
    if (strcmp(start_success, "start!") == 0)
    {
        printf("Genshin impact: start success!");
    }
    else
    {
        printf("Genshin impact: start failed!");
    }

    int j = 10;

    while (1)
    {
        for (int i = 0; i < 50; i++)
        {
            /* move */
            sys_move_cursor(i, j + 0);
            printf("%s", plane1);

            sys_move_cursor(i, j + 1);
            printf("%s", plane2);

            sys_move_cursor(i, j + 2);
            printf("%s", plane3);

            sys_move_cursor(i, j + 3);
            printf("%s", plane4);

            sys_move_cursor(i, j + 4);
            printf("%s", plane5);

            sys_move_cursor(i, j + 5);
            printf("%s", plane6);

            sys_move_cursor(i, j + 6);
            printf("%s", plane7);
        }
        // sys_yield();

        sys_move_cursor(0, j + 0);
        printf("%s", blank);

        sys_move_cursor(0, j + 1);
        printf("%s", blank);

        sys_move_cursor(0, j + 2);
        printf("%s", blank);

        sys_move_cursor(0, j + 3);
        printf("%s", blank);

        sys_move_cursor(0, j + 4);
        printf("%s", blank);

        sys_move_cursor(0, j + 5);
        printf("%s", blank);

        sys_move_cursor(0, j + 6);
        printf("%s", blank);
    }

    return 0;
}
