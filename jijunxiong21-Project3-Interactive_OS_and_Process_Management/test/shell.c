/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#define INPUT_BUF_SIZE 80
#define SHELL_BEGIN    40
#define SHELL_HEIGHT   20
#define SCREEN_WIDTH   100
#define SCREEN_HEIGHT  80
#define SCREEN_LOC(x, y) ((y) * SCREEN_WIDTH + (x)) // location of (x, y) in screen buffer
#define ARGV_MAX_SIZE  256 // max size of argv stack (in bytes) (consists of pointer and memory space)

// max argv size: [input buffer size] * (16/2) = 400

// #define CSI_0           '\033' // Control Sequence Introducer 1st char
// #define CSI_1           '['    // Control Sequence Introducer 2nd char

char shell_buf[SHELL_HEIGHT][SCREEN_WIDTH + 1] = {0}; // new screen buffer with empty spaces
char *shell_buf_ptr[SHELL_HEIGHT];
char buf[INPUT_BUF_SIZE] = {0};
char buf_mod[INPUT_BUF_SIZE] = {0};
// char blank[INPUT_BUF_SIZE + 1] = {
//     [0 ... INPUT_BUF_SIZE - 1] = ' ',
//     [INPUT_BUF_SIZE] = '\0'
// };
int shell_cursor_y = SHELL_BEGIN;
// int output_cursor_y = 0;
char strbuf[SCREEN_HEIGHT] = {0}; //

bool no_wait;
bool in_quote;
bool next_cmd_exist = false;
int next_cmd_idx;

// const char *cmd_name[] = {
//     "exec",
//     "ps",
//     "kill",
//     "clear",
// };

extern long atol(const char *str);
extern int atoi(const char *str);

int getline(char *buf, int bufsize)
{
    int ch, idx = 0;
    while (1) { // you can modify the condition here to quit the loop
        ch = sys_getchar(); // get input character
        if (ch == -1) // if input is invalid, continue
        {
            continue;
        }
        else if (ch == '\b') // if input is backspace
        {
            if (idx > 0)
            {
                printf("\b"); // reprint the input string
                --idx;
            }
        }
        else if (ch == '\r' || ch == '\n') // if input is enter
        {
            printf("\n");
            // if (idx == 0)
            //     continue;
            buf[idx] = '\0';
            break; // the only way to quit the loop
        }
        else if (idx == bufsize - 1) // if input is too long, give a warning and reprint the input string
        {
            printf("  [ERROR] > Input too long!\r");
            printf(buf);
        }
        else // if input is other character (valid, so that ch != -1)
        {
            printf("%c", ch);
            buf[idx++] = ch;
        }
    }
    return idx;
}

// // move all the lines in shell_buf up by 1 line
// void shell_moveup(void)
// {
//     char *shell_buf_ptr_0 = shell_buf_ptr[0];
//     for (int i = 0; i < SHELL_HEIGHT - 1; i++)
//     {
//         shell_buf_ptr[i] = shell_buf_ptr[i + 1];
//         sys_move_cursor(0, SHELL_BEGIN + i + 1);
//         printf("%s", blank);
//         sys_move_cursor(0, SHELL_BEGIN + i + 1);
//         printf("%s", shell_buf_ptr[i]);
//         // printsh(shell_buf_ptr[i]);
//     }
//     shell_buf_ptr[SHELL_HEIGHT - 1] = shell_buf_ptr_0;
//     sys_move_cursor(0, SHELL_BEGIN + SHELL_HEIGHT);
//     printf("%s", blank);
//     sys_move_cursor(0, SHELL_BEGIN + SHELL_HEIGHT);
// }

int printsh(const char *fmt)
{
    int ret = 0;

    if (shell_cursor_y <= SHELL_BEGIN + SHELL_HEIGHT - 1)
    {
        shell_cursor_y++;
        sys_move_cursor(0, shell_cursor_y);
    }
    else
        sys_shell_moveup(shell_buf_ptr);
    strcpy(shell_buf_ptr[shell_cursor_y - SHELL_BEGIN - 1], fmt);
    ret = printf(fmt);
    return ret;
}

int parse_input(int input_len)
{
    // note: backspace maybe 8('\b') or 127(delete)

    int input_idx;
    int modify_idx;
    int cmd_argc = -1;

    no_wait = false;
    in_quote = false;
    next_cmd_exist = false;
    next_cmd_idx = 0;

    // buf_ptr = ;
    // argv_ptr = (char *)(cmd_argv) + (cmd_argc - 1) * sizeof(char *);

    for (input_idx = modify_idx = 0; input_idx < input_len + 1 && next_cmd_idx == 0;
            ++input_idx, ++modify_idx)
    {
        // if (in_quote && buf[input_idx] != '"')
        // {
        //     buf_mod[modify_idx] = buf[input_idx];
        //     continue;
        // }
        switch (buf[input_idx])
        {
        case ' ': case '\t': case '\0':
            // buf[input_idx] = '\0';
            if (modify_idx != 0 && buf_mod[modify_idx - 1] != '\0')
            {
                cmd_argc++;
                buf_mod[modify_idx] = '\0';
            }
            else
                --modify_idx;
            break;

        // case '"':
        //     in_quote = !in_quote;
        //     break;

        case '&':
            // buf[input_idx] = '\0';
            if (modify_idx != 0 && buf_mod[modify_idx - 1] != '\0')
            {
                cmd_argc++;
                buf_mod[modify_idx] = '\0';
            }
            else
                --modify_idx;
            if (input_idx + 1 >= input_len || buf[input_idx + 1] != '&')
                no_wait = true;
            else
                input_idx++;
            // while (input_idx < input_len && (buf[input_idx] == ' ' || buf[input_idx] == '\t'))
            //     input_idx++;
            if (input_idx < input_len)
            {
                next_cmd_exist = true;
                next_cmd_idx = input_idx + 1;
            }
            break;

        default:
            buf_mod[modify_idx] = buf[input_idx];
            break;
        }
    }
    return cmd_argc;
}


int main(void)
{
    for (int i = 0; i < SHELL_HEIGHT; i++)
    {
        shell_buf_ptr[i] = shell_buf[i];
    }

    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");

    int input_len;

    int cmd_argc;
    void *cmd_argv[ARGV_MAX_SIZE];



    // char *argv_ptr;
    // char *argv_base;

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        if (!next_cmd_exist) // if no next command (no & or &&)
        {
            // check command line buffer of the shell
            if (shell_cursor_y <= SHELL_BEGIN + SHELL_HEIGHT - 1)
            {
                shell_cursor_y++;
                sys_move_cursor(0, shell_cursor_y);
            }
            else
                sys_shell_moveup(shell_buf_ptr);
            printf("> root@UCAS_OS: ");
            input_len = getline(buf, INPUT_BUF_SIZE);
            sys_move_cursor(0, shell_cursor_y);            
            strcpy(shell_buf_ptr[shell_cursor_y - SHELL_BEGIN - 1], "> root@UCAS_OS: ");
            strcpy(shell_buf_ptr[shell_cursor_y - SHELL_BEGIN - 1] + sizeof("> root@UCAS_OS: ") - 1, buf);
        }
        else // if last command line has & or &&, then read begining at next_cmd_idx
        {
            input_len = strlen(buf + next_cmd_idx);
            buf_mod[0] = '\0';
            strncpy(buf, buf + next_cmd_idx, INPUT_BUF_SIZE - next_cmd_idx + 1);
            next_cmd_exist = false;
        }


        // TODO [P3-task1]: parse input
        cmd_argc = parse_input(input_len);
        if (cmd_argc < 0) // if input is empty, continue
            continue;


        // done in getline. see getline() above this function
        char pid_str[11]; // pid_t is int32_t, so 11 bytes is enough

        // check command line buffer of the shell (2nd time)

        // TODO [P3-task1]: ps, exec, kill, clear
        if (strncmp(buf_mod, "ps", sizeof("ps")) == 0)
        {
            if (cmd_argc != 0)
                printsh("ps: error: too many arguments!");
            else
            {
                sys_ps(); // automatically move the cursor
                sys_move_cursor(0, shell_cursor_y - 1);
            }
        }
        else if (strncmp(buf_mod, "exec", sizeof("exec")) == 0)
        {
            if (cmd_argc < 1)
                printsh("exec: error: expected task name");
            else
            {
                // cmd_argv: the
                char *name = buf_mod + 5;
                char *buf_ptr = name;

                for (int i = 0; i < cmd_argc; i++)
                {
                    // the i-1th (char *) pointer, points to the ith string
                    *((char **)(cmd_argv) + i) = buf_ptr;
                    buf_ptr += strlen(buf_ptr) + 1;
                }
                *((char **)(cmd_argv) + cmd_argc) = NULL;

                pid_t pid = sys_exec(name, cmd_argc, (char **)cmd_argv);
                itoa(pid, pid_str, 23, 10);
                if (pid == 0)
                {
                    strcpy(strbuf, "exec: error: task not found.");
                    // strcat(strbuf, pid_str);
                    printsh(strbuf);
                }
                else
                {
                    strcpy(strbuf, "exec: successfully created task, pid = ");
                    strcat(strbuf, pid_str);
                    printsh(strbuf);
                }
                if (no_wait == 0)
                    sys_waitpid(pid);
            }
        }
        else if (strncmp(buf_mod, "kill", sizeof("kill")) == 0)
        {
            if (cmd_argc != 1)
                printsh("kill: error: expect a pid");
            else
            {
                pid_t pid = atoi(buf_mod + 5);
                if (pid == 0)
                    printsh("kill: error: invalid pid");
                else
                {
                    itoa(pid, pid_str, 23, 10);
                    if (sys_kill(pid))
                    {
                        strcpy(strbuf, "kill: successfully killed task, pid = ");
                        strcat(strbuf, pid_str);
                        printsh(strbuf);
                    }
                    else
                    {
                        strcpy(strbuf, "kill: error: task not found");
                        // strcat(strbuf, pid_str);
                        printsh(strbuf);
                    }
                }
            }
        }
        else if (strncmp(buf_mod, "clear", sizeof("clear")) == 0)
        {
            if (cmd_argc != 0)
                printsh("clear: error: no argument expected");
            else
            {
                sys_clear();
                shell_cursor_y = SHELL_BEGIN;
                sys_move_cursor(0, SHELL_BEGIN);
                printf("------------------- COMMAND -------------------\n");
            }
        }
        else if (strncmp(buf_mod, "taskset", sizeof("taskset")) == 0)
        {
            if (cmd_argc < 2)
            {
                printsh("taskset: error: need at least 2 arguments");
            }
            else
            {
                // cmd_argv: the
                char *buf_ptr = buf_mod + sizeof("taskset");

                for (int i = 0; i < cmd_argc; i++)
                {
                    // the i-1th (char *) pointer, points to the ith string
                    *((char **)(cmd_argv) + i) = buf_ptr;
                    buf_ptr += strlen(buf_ptr) + 1;
                }
                *((char **)(cmd_argv) + cmd_argc) = NULL;

                if (strncmp(cmd_argv[0], "-p", 3) == 0)
                {
                    if (cmd_argc < 3)
                        printsh("taskset: error: \"-p\" option need at least 3 arguments");
                    else
                    {
                        int mask = atoi(cmd_argv[1]);
                        pid_t pid = atoi(cmd_argv[2]);
                        if (sys_taskset(mask, pid, NULL, 0, NULL) != 0)
                        {
                            printsh("taskset: error: argument wrong.");
                        }
                        else
                        {
                            strcpy(strbuf, "taskset: set the mask of pid==");
                            strcat(strbuf, cmd_argv[2]);
                            strcat(strbuf, " to ");
                            strcat(strbuf, cmd_argv[1]);
                            printsh(strbuf);
                        }
                    }
                }
                else
                {
                    if (cmd_argc < 2)
                        printsh("taskset: error: need at least 2 arguments");
                    else
                    {
                        int mask = atoi(cmd_argv[0]);
                        if (sys_taskset(mask, 0, cmd_argv[1], cmd_argc - 1, (char **)cmd_argv + 1) != 0)
                        {
                            printsh("taskset: error: argument wrong.");
                        }
                        else
                        {
                            strcpy(strbuf, "taskset: set the mask of ");
                            strcat(strbuf, cmd_argv[1]);
                            strcat(strbuf, " to ");
                            strcat(strbuf, cmd_argv[0]);
                            printsh(strbuf);
                        }
                    }
                }
            }
        }
        else
        {
            printsh("shell: error: Unknown command!");
        }


        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}
