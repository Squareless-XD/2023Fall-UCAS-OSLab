#ifndef INCLUDE_STDIO_H_
#define INCLUDE_STDIO_H_

#include <stdarg.h>
#include <stdint.h>

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list va);

void *my_malloc(size_t size);
void my_free(void *ptr);

#endif
