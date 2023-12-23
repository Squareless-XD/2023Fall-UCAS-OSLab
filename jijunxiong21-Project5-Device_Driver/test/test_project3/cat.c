#include <stdio.h>
#include <unistd.h>

/**
 * The ascii cat is designed
 * from: https://www.asciiart.eu/animals/cats
 */

static char blank[] = {"                                                "};
static char cat1[]  = {"     \\    /\\"};
static char cat2[]  = {"       )  ( ')"};
static char cat3[]  = {"      (  /  )"};
static char cat4[]  = {"       \\(__)|"};

/**
 * NOTE: bios APIs is used for p2-task1 and p2-task2. You need to change
 * to syscall APIs after implementing syscall in p2-task3!
*/
int main(void)
{
    int j = 4;

    while (1)
    {
        for (int i = 0; i < 35; i++)
        {
            /* move */
            sys_move_cursor(i, j + 0);
            printf("%s", cat1);

            sys_move_cursor(i, j + 1);
            printf("%s", cat2);

            sys_move_cursor(i, j + 2);
            printf("%s", cat3);

            sys_move_cursor(i, j + 3);
            printf("%s", cat4);
        }
        sys_move_cursor(0, j + 0);
        printf("%s", blank);

        sys_move_cursor(0, j + 1);
        printf("%s", blank);

        sys_move_cursor(0, j + 2);
        printf("%s", blank);

        sys_move_cursor(0, j + 3);
        printf("%s", blank);
    }
}
