#include <stdio.h>
#include <unistd.h>

/**
 * The ascii dinosaur is designed by myself
 */

static char blank[] = {"                                                             "};
// "             _______"    "             _______"    "             _______"   
// "            /       \ "  "            /       \ "  "            /       \ " 
// "           |  LI     |"  "           |  LI     |"  "           |  LI     |" 
// "           |     ____|"  "           |     ____|"  "           |     ____|" 
// " L       _P     ^----  " " L       _P     ^----  " " L       _P     ^----  "
// " \\___nP^       /"       " \\___nP^       /"       " \\___nP^       /"      
// "  \            |**$ "    "  \            |**$ "    "  \            |**$ "   
// "   \           |  '"     "   \           |  '"     "   \           |  '"    
// "    q  _____  /"         "    q  _____  /"         "    q  _____  /"        
// "     |(     P("          "     |(     IL"          "     IL     P("         
// "     |VL    L\"          "     |VL      "          "            L\"         

static char dinosaur1[]    = {"              _______"};
static char dinosaur2[]    = {"             /       \\ "};
static char dinosaur3[]    = {"            |  LI     |"};
static char dinosaur4[]    = {"            |     ____|"};
static char dinosaur5[]    = {"  L       _P     ^----  "};
static char dinosaur6[]    = {"  \\\\___nP^       /"};
static char dinosaur7[]    = {"   \\            |**$ "};
static char dinosaur8[]    = {"    \\           |  '"};
static char dinosaur9[]    = {"     q  ____   /"};

static char dinosaur10[]   = {"      |(     P("};
static char dinosaur11[]   = {"      |VL    L\\"};
static char dinosaur10_1[] = {"      |(     IL"};
static char dinosaur11_1[] = {"      |VL      "};
static char dinosaur10_2[] = {"      IL     P("};
static char dinosaur11_2[] = {"             L\\"};

static char earth1[] = {"_____________________________________________________________"};
static char earth2[] = {"       -  _        _                  ~~              ~      "};
static char earth3[] = {"        _                ~~~                    -            "};
static char earth4[] = {"^        ==         __         =                 - ==        "};


static char *dinosaur[] = {
    dinosaur1, dinosaur2, dinosaur3, dinosaur4, dinosaur5, dinosaur6, dinosaur7, dinosaur8, dinosaur9, dinosaur10, dinosaur11
};
static char *earth[] = {
    earth1, earth2, earth3, earth4
};
/**
 * NOTE: bios APIs is used for p2-task1 and p2-task2. You need to change
 * to syscall APIs after implementing syscall in p2-task3!
*/
int main(void)
{
    int j = 10;

    while (1)
    {
        for (int k = 0; k < 4; k++) {
            sys_move_cursor(0, j + k + 11);
            printf("%s", earth[k]);
        }
        for (int i = 0; i < 20; i++)
        {
            switch (i % 8) {
            case 1:
                dinosaur[9 ] = dinosaur10_1;
                dinosaur[10] = dinosaur11_1;
                break;
            case 5:
                dinosaur[9 ] = dinosaur10_2;
                dinosaur[10] = dinosaur11_2;
                break;
            case 3: case 7:
                dinosaur[9 ] = dinosaur10;
                dinosaur[10] = dinosaur11;
                break;
            default:
                break;
            }
            /* move */
            for (int k = 0; k < 11; k++) {
                sys_move_cursor(2 * i, j + k);
                printf("%s", dinosaur[k]);
            }
        }
        // sys_yield();

        for (int k = 0; k < 11; k++) {
            sys_move_cursor(0, j + k);
            printf("%s", blank);
        }
    }
}
