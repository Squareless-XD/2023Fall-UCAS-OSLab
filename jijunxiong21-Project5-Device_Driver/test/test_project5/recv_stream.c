#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

// #define MAX_RECV_CNT 32
#define RX_PKT_SIZE 0x10000

uint32_t recv_buffer[RX_PKT_SIZE];
int recv_length;

uint16_t fletcher16(uint8_t *data, int n)
{
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;

    int i;
    for (i = 0; i < n; i++)
    {
        sum1 = (sum1 + data[i]) % 0xff;
        sum2 = (sum2 + sum1) % 0xff;
    }
    return (sum2 << 8) | sum1;
}

int main(void)
{
    int print_location = 1;
    int iteration = 1;

    while (1)
    {
        sys_move_cursor(0, print_location);
        printf("[RECV_STREAM] start! ");

        recv_length = RX_PKT_SIZE;

        int ret = sys_net_recv_stream(recv_buffer, &recv_length);
        printf("size: %d, iteration: %d\n    ", ret, iteration++);

        // calculate fletcher16 and print it
        uint16_t checksum = fletcher16((uint8_t *)recv_buffer, recv_length);
        printf("length of data: %05d  checksum: %04x\n", recv_length, checksum);
    }

    return 0;
}

/*
loadbootm
exec fly &
exec recv_stream &

loadbootm
exec recv_stream &
*/
