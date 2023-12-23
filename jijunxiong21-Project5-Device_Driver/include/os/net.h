#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

// #include <os/list.h>
// #include <type.h>
#include <lib.h>
#include <os/lock.h>

#define PKT_NUM 32

extern struct list_node send_block_queue;
extern struct list_node recv_block_queue;
extern struct list_node recv_stream_block_queue;

void net_handle_irq(void);
int do_net_send(void *txpacket, int length);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_recv_stream(void *buffer, int *nbytes);
void e1000_handle_txqe(void);
void e1000_handle_rxdmt0(void);
void check_recv_stream();

#endif  // !__INCLUDE_NET_H__