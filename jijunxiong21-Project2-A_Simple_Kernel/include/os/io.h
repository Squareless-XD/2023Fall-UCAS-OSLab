// This file defines the data structure and functions for process stack management
// The process stack is allocated in the user memory space
// NOTE: this file is added by the student

#ifndef INCLUDE_IO_H_
#define INCLUDE_IO_H_

#include <type.h>

#define INPUT_BUF_SIZE 50

extern char buf[INPUT_BUF_SIZE];

// get a string from the console, and return the length of the string.
// if longer than bufsize, return -1
extern int getline(char *buf, int bufsize);


#endif
