#include "tinylibdeflate.h"
// #include <stdlib.h>
// #include <stdio.h>
// #include <string.h>

#define BUFFER_SIZE 100000
#define ALLOC_BUF 0x52080000

unsigned long long *alloc_buf_ptr = (void *)ALLOC_BUF;
unsigned long long buf_off = 0;

// malloc and free are used by libdeflate
void *malloc(long long size)
{
    void *ret = alloc_buf_ptr + buf_off;
    buf_off += (size / 8) + (size % 8 != 0) ? 1 : 0; // 8-byte aligned
    return ret;
}

void free(void *ptr)
{
    return;
}

int main(char *data_in, long long len_in, char *data_out, int *len_out){
    // data to be compressed
    // char data[BUFFER_SIZE] = "This is an example for DEFLATE compression and decompression!\0";
    // char compressed[BUFFER_SIZE];
    // char extracted[BUFFER_SIZE];
    // int data_len = strlen(data);

    // prepare environment
    deflate_set_memory_allocator((void * (*)(int))malloc, free);
    // struct libdeflate_compressor * compressor = deflate_alloc_compressor(1);
    struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();

    // // do compress
    // int out_nbytes = deflate_deflate_compress(compressor, data, data_len, compressed, BUFFER_SIZE);
    // printf("original: %d\n", data_len);
    // printf("compressed: %d\n", out_nbytes);

    // do decompress
    // int restore_nbytes = 0;
    if(deflate_deflate_decompress(decompressor, data_in, len_in, data_out, BUFFER_SIZE, len_out)){
        // printf("An error occurred during decompression.\n");
        // exit(1);
    }
    // printf("decompressed: %d\n%s\n", restore_nbytes, extracted);

    return 0;
}