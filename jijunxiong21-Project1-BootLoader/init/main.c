#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// get app info address from memory
short info_off;
int task_num;

// Task info array
task_info_t tasks[TASK_MAXNUM];
int exists_in_mem[TASK_MAXNUM];

int (*decompress)(char *data_in, long long len_in, char *data_out, long long *len_out) = (void *)DECOMPRESS;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first

    // get app info from memory
    info_off = *(short *)APP_INFO_OFF_ADDR;
    int info_size = sizeof(task_info_t) * TASK_MAXNUM + sizeof(int); // tasks & task_num

    // NOTE: task_info_t is 4-byte aligned?
    int info_sec_begin = info_off / SECTOR_SIZE;
    int info_sec_end = NBYTES2SEC(info_off + info_size);
    int info_sec_num = info_sec_end - info_sec_begin;
    bios_sd_read(INITIAL_BUF, info_sec_num, info_sec_begin);

    // load task_num
    int *task_num_addr = (int *)(INITIAL_BUF + info_off % SECTOR_SIZE);
    task_num = *task_num_addr;

    // get app info from sd card
    /* one bug here: too much trust for "plus" */
    task_info_t *info_addr = (task_info_t *)((void *)task_num_addr + sizeof(int));
    for (int task_idx = 0; task_idx < TASK_MAXNUM; ++task_idx)
    {
        tasks[task_idx] = info_addr[task_idx];
    }
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);


    // echo the input character to the screen (p1-task2)
    // int ch;
    // while (ch = bios_getchar())
    // {
    //     if (ch == '\r')
    //     {
    //         bios_putchar('\n');
    //     }
	//     else if (ch != -1)
    //     {
    //         bios_putchar(ch);
    //     }
    // }

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    int ch;
    int input_idx = 0; // record the input string index
    void (*entry_point)(void);

    buf[VERSION_BUF] = '\0'; // set the end of the buffer to '\0'

    while (ch = bios_getchar())
    {
        // if input is backspace
        if (ch == '\b')
        {
            if (input_idx > 0)
            {
                // bios_putchar(ch); // echo backspace
                // reprint the input string
                bios_putchar('\b');
                bios_putchar(' ');
                bios_putchar('\b');

                --input_idx;
            }
        }

        // if input is too long, give a warning and reprint the input string
        else if (input_idx == VERSION_BUF - 1)
        {
            bios_putchar('\n');
            bios_putstr("Input too long!\n");
            bios_putstr(buf);
        }

        // if input is enter
        else if (ch == '\r')
        {
            bios_putchar('\n');
            if (input_idx == 0) // if input is empty, dont take it as a task name
            {
                continue;
            }
            buf[input_idx] = '\0';
            input_idx = 0;
            int task_found = false;
            for (int task_idx = 0; task_idx < task_num; ++task_idx)
            {
                if (strcmp(buf, tasks[task_idx].task_name) == 0) // find the task
                {
                    // set the flag
                    task_found = true;
                    bios_putstr("Task found!\n");

                    // load and execute the task
                    entry_point = load_task_img(task_idx);
                    entry_point();
                    break;
                }
            }
            if (!task_found)
            {
                bios_putstr("Task not found!\n");
            }
        }

        // if input is other character (valid, so that ch != -1)
        else if (ch != -1)
        {
            bios_putchar(ch);
            buf[input_idx] = ch;
            ++input_idx;
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
