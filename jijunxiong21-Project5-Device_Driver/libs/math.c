#include <math.h>

int min(int a, int b)
{
    return a < b ? a : b;
}

int max(int a, int b)
{
    return a > b ? a : b;
}

int round_up_div(int num, int divisor)
{
    return (num + divisor - 1) / divisor;
}
