#include <screen.h>
#include <os/sched.h>
#include <os/kernel.h>

/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0}; // new screen buffer with empty spaces
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0}; // old screen buffer with empty spaces

// screen buffer locks
spin_lock_t screen_lock = {UNLOCKED};

// move cursor to (x, y)
static void vt100_move_cursor(int x, int y)
{
    // \033[y;xH
    printv("%c%c%d;%dH", CSI_0, CSI_1, y, x);
}

// clear screen
static void vt100_clear()
{
    // \033[2J
    printv("%c%c2J", CSI_0, CSI_1);
}

// hide cursor
static void vt100_hidden_cursor()
{
    // \033[?25l
    printv("%c%c?25l", CSI_0, CSI_1);
}
/*
// show cursor
static void vt100_shown_cursor()
{
    // \033[?25l
    printv("%c%c?25h", CSI_0, CSI_1);
}
*/
// write a char to screen
static void screen_write_ch(char ch)
{
    spin_lock_acquire(&screen_lock); /* enter screen buffer critical section */
    pcb_t *cpu_proc = my_cpu();

    if (ch == '\n')
    {
        cpu_proc->cursor_x = 0; // move cursor to the beginning
        cpu_proc->cursor_y++;   // move cursor to the next line
    }
    else if (ch == '\r')
    {
        cpu_proc->cursor_x = 0; // move cursor to the beginning
        // cpu_proc->cursor_y++;   // do not move cursor to the next line
    }
    else if (ch == '\b')
    {
        printv("%c%c1X", CSI_0, CSI_1);
        if (cpu_proc->cursor_x > 0)
            cpu_proc->cursor_x--; // move cursor to the beginning
        new_screen[SCREEN_LOC(cpu_proc->cursor_x, cpu_proc->cursor_y)] = ' '; // write empty space to screen buffer
    }
    else
    {
        new_screen[SCREEN_LOC(cpu_proc->cursor_x, cpu_proc->cursor_y)] = ch; // write char to screen buffer
        cpu_proc->cursor_x++; // move cursor to the next column
    }

    spin_lock_release(&screen_lock); /* leave screen buffer critical section */
}

void init_screen(void)
{
    spin_lock_acquire(&screen_lock); /* enter screen buffer critical section */
    vt100_hidden_cursor();  // hide cursor
    vt100_clear();          // clear screen
    spin_lock_release(&screen_lock); /* leave screen buffer critical section */
    screen_clear();         // clear screen buffer
}

// clear screen buffer
void screen_clear(void)
{
    int i, j;
    pcb_t *cpu_proc = my_cpu();
    spin_lock_acquire(&screen_lock); /* enter screen buffer critical section */

    // fill screen buffer with empty spaces
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = ' ';
        }
    }

    // reset cursor position
    cpu_proc->cursor_x = 0;
    cpu_proc->cursor_y = 0;
    spin_lock_release(&screen_lock); /* leave screen buffer critical section */

    screen_reflush();
}

// move cursor to (x, y)
void screen_move_cursor(int x, int y)
{
    spin_lock_acquire(&screen_lock); /* enter screen buffer critical section */
    pcb_t *cpu_proc = my_cpu();
    cpu_proc->cursor_x = x;
    cpu_proc->cursor_y = y;
    vt100_move_cursor(x + 1, y + 1);
    spin_lock_release(&screen_lock); /* leave screen buffer critical section */
}

// write a string to screen
void screen_write(char *buff)
{
    int i = 0;
    int l = strlen(buff);

    for (i = 0; i < l; i++)
    {
        screen_write_ch(buff[i]);
    }
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void)
{
    int i, j;
    spin_lock_acquire(&screen_lock); /* enter screen buffer critical section */

    /* here to reflush screen buffer to serial port */
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            /* We only print the data of the modified location. */
            if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)])
            {
                // do a mapping from (0, 0) to (1, 1), for in vt 100, screen index
                // starts from (1, 1), not (0, 0) as in C. our screen buffer is
                // (0, 0) based, so we need to add 1 to both x and y.
                // also, j is x, i is y
                vt100_move_cursor(j + 1, i + 1);
                bios_putchar(new_screen[SCREEN_LOC(j, i)]);
                old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
            }
        }
    }

    /* recover cursor position */
    pcb_t *cpu_proc = my_cpu();
    vt100_move_cursor(cpu_proc->cursor_x + 1, cpu_proc->cursor_y + 1);

    spin_lock_release(&screen_lock); /* leave screen buffer critical section */

    // DON'T FORGET TO ADD 1 TO BOTH X AND Y HERE
}

/*
    Valid ANSI Mode Control Sequences:

    In the VT100 the CSI (Control Sequence Introducer) is `ESC [`
    (left bracket, ASCII 91). Here is a partial list of the most common
    \033[0m     Resey
    \033[1m     Bold
    \033[2m     Faint
    \033[3m     Italic
    \033[4m     Underline
    \033[5m     Slow blink
    \033[6m     Fast blink
    \033[7m     Reverse video
    \033[8m     Erase
    \033[9m     Strikethrought

    \033[21m    Double underline
    \033[22m    Bold: off
    \033[23m    Italic: off
    \033[24m    Underline: off
    \033[25m    Slow blink: off
    \033[26m    Fast blink: off
    \033[27m    Reverse video: off
    \033[28m    Erase: off
    \033[29m    Strikethrought: off
    \033[30m    Foreground: black      RGB:  Normal: 000000  Bold: 555555
    \033[31m    Foreground: red        RGB:  Normal: AA0000  Bold: FF5555
    \033[32m    Foreground: green      RGB:  Normal: 00AA00  Bold: 55FF55
    \033[33m    Foreground: yellow     RGB:  Normal: AAAA00  Bold: FFFF55
    \033[34m    Foreground: blue       RGB:  Normal: 0000AA  Bold: 5555FF
    \033[35m    Foreground: magenta    RGB:  Normal: AA00AA  Bold: FF55FF
    \033[36m    Foreground: cyan       RGB:  Normal: 00AAAA  Bold: 55FFFF
    \033[37m    Foreground: white      RGB:  Normal: AAAAAA  Bold: FFFFFF
    \033[38;2;nm Foreground: 24 bit rgb color code
    \033[38;5;nm Foreground: 8 bit 256 color mode
    \033[39m    Foreground: reset
    \033[40m    Background: black
    \033[41m    Background: red
    \033[42m    Background: green
    \033[43m    Background: yellow
    \033[44m    Background: blue
    \033[45m    Background: magenta
    \033[46m    Background: cyan
    \033[47m    Background: white
    \033[48;2;nm Background: 24 bit rgb color code
    \033[48;5;nm Background: 8 bit 256 color mode
    \033[49m    Background: reset

    \033[n@     Insert n blank characters
    \033[nA     Move cursor up n rows
    \033[nB     Move cursor down n rows
    \033[nC     Move cursor forward n columns
    \033[nD     Move cursor backward n columns
    \033[nE     Move cursor to beginning of line n lines down
    \033[nF     Move cursor up n rows, to column 1
    \033[nG     Move cursor to column n
    \033[n;mH   Move cursor to row n, and column m
    \033[nJ     0: erase from cursor to end of display,
                1: erase from start of display to cursor,
                2: erase display
    \033[nK     0: erase from cursor to end of line,
                1: erase from start of line to cursor,
                2: erase line
    \033[nL     Insert n blank lines
    \033[nM     Delete n lines
    \033[nP     Delete n characters
    \033[nX     Erase n characters
    \033[n;mr   Set scrolling region between top row n and bottom row m
*/

const char blank[INPUT_BUF_SIZE + 1] = {
    [0 ... INPUT_BUF_SIZE - 1] = ' ',
    [INPUT_BUF_SIZE] = '\0'
};

// move all the lines in shell_buf up by 1 line
void shell_moveup(char *shell_buf_ptr[SHELL_HEIGHT])
{
    char *shell_buf_ptr_0 = shell_buf_ptr[0];
    for (int i = 0; i < SHELL_HEIGHT - 1; i++)
    {
        shell_buf_ptr[i] = shell_buf_ptr[i + 1];
        screen_move_cursor(0, SHELL_BEGIN + i + 1);
        printk("%s\r%s", blank, shell_buf_ptr[i]);
    }
    shell_buf_ptr[SHELL_HEIGHT - 1] = shell_buf_ptr_0;
    screen_move_cursor(0, SHELL_BEGIN + SHELL_HEIGHT);
    printk("%s\r", blank);
    // screen_move_cursor(0, SHELL_BEGIN + SHELL_HEIGHT);
}
