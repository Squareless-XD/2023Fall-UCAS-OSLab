#include <stdio.h>
#include <unistd.h>

/**
 * The ascii airplane is designed
 * from: https://www.asciiart.eu/vehicles/airplanes
 */

static char blank[] = {"                                                                                  "};
static char plane1[]  = {"                 .------,"};
static char plane2[]  = {"                 =\\      \\"};
static char plane3[]  = {"    .---.         =\\      \\"};
static char plane4[]  = {"    | C~ \\         =\\      \\"};
static char plane5[]  = {"    |     `----------'------'----------,"};
static char plane6[]  = {"   .'     LI.-.LI LI LI LI LI LI LI.-.LI`-."};
static char plane7[]  = {"   \\ _/.____|_|______.------,______|_|_____)"};
static char plane8[]  = {"                    /      /"};
static char plane9[]  = {"                  =/      /"};
static char plane10[] = {"                 =/      /"};
static char plane11[] = {"                =/      /"};
static char plane12[] = {"                /_____,'"};
/**
 * NOTE: bios APIs is used for p2-task1 and p2-task2. You need to change
 * to syscall APIs after implementing syscall in p2-task3!
*/
int main(void)
{
    int j = 10;

    while (1)
    {
        for (int i = 0; i < 13; i++)
        {
            /* move */
            sys_move_cursor(3 * i, j + 0);
            printf("%s", plane1);

            sys_move_cursor(3 * i, j + 1);
            printf("%s", plane2);

            sys_move_cursor(3 * i, j + 2);
            printf("%s", plane3);

            sys_move_cursor(3 * i, j + 3);
            printf("%s", plane4);

            sys_move_cursor(3 * i, j + 4);
            printf("%s", plane5);

            sys_move_cursor(3 * i, j + 5);
            printf("%s", plane6);

            sys_move_cursor(3 * i, j + 6);
            printf("%s", plane7);

            sys_move_cursor(3 * i, j + 7);
            printf("%s", plane8);

            sys_move_cursor(3 * i, j + 8);
            printf("%s", plane9);

            sys_move_cursor(3 * i, j + 9);
            printf("%s", plane10);

            sys_move_cursor(3 * i, j + 10);
            printf("%s", plane11);

            sys_move_cursor(3 * i, j + 11);
            printf("%s", plane12);
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

        sys_move_cursor(0, j + 7);
        printf("%s", blank);

        sys_move_cursor(0, j + 8);
        printf("%s", blank);

        sys_move_cursor(0, j + 9);
        printf("%s", blank);

        sys_move_cursor(0, j + 10);
        printf("%s", blank);

        sys_move_cursor(0, j + 11);
        printf("%s", blank);
    }
}
