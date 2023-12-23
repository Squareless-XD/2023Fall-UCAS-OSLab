#include <os/kernel.h>
#include <os/io.h>
#include <screen.h>
#include <printk.h>
#include <os/sched.h>

char buf[INPUT_BUF_SIZE] = {0};

int getline(char *buf, int bufsize)
{
    int ch, idx = 0;
    while (true) { // you can modify the condition here to quit the loop
        ch = bios_getchar(); // get input character
        if (ch == -1) { // if input is invalid, continue
            continue;
        }
        else if (ch == '\b') { // if input is backspace
            if (idx > 0) {
                printk("\b"); // reprint the input string
                --idx;
            }
        }
        else if (ch == '\r' || ch == '\n') { // if input is enter
            printk("\n");
            if (idx == 0)
                continue;
            buf[idx] = '\0';
            break; // the only way to quit the loop
        }
        else if (idx == bufsize - 1) { // if input is too long, give a warning and reprint the input string
            printk("\nInput too long!\n");
            printk(buf);
        }
        else { // if input is other character (valid, so that ch != -1)
            printk("%c", ch);
            buf[idx++] = ch;
        }
    }
    return idx;
}
