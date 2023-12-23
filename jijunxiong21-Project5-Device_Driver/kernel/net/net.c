#include <e1000.h>
// #include <type.h>
// #include <os/irq.h>
#include <os/net.h>
#include <os/task_tools.h>
// #include <os/string.h>
// #include <os/list.h>
// #include <os/smp.h>
#include <os/mm.h>
#include <os/time.h>
#include <endian.h>

LIST_HEAD(send_block_queue);
LIST_HEAD(recv_block_queue);
LIST_HEAD(recv_stream_block_queue);

static spin_lock_t net_send_lk = {UNLOCKED};
static spin_lock_t net_recv_lk = {UNLOCKED};

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full

    spin_lock_acquire(&net_send_lk);

    // wait until the transmit queue is not full
    while (e1000_transmit(txpacket, length) < 0) {
        ;
        // #define E1000_ICR               0x000C0	/* Interrupt Cause Read - R/clr */
        // #define E1000_IMS               0x000D0	/* Interrupt Mask Set - RW */
        // #define E1000_IMC               0x000D8	/* Interrupt Mask Clear - WO */
        // #define E1000_ICR_TXQE	  0x00000002	/* Transmit Queue empty */
        // #define E1000_IMS_TXQE	  E1000_ICR_TXQE	/* Transmit Queue empty */
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        local_flush_dcache();
        WRITE_LOG("put into send block queue\n")
        do_block(&send_block_queue, &net_send_lk);
    }

    spin_lock_release(&net_send_lk);

    return 0;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    int length;
    void *rx_pkt_buf = rxbuffer;
    spin_lock_acquire(&net_recv_lk);

    // wait until the transmit queue is not full
    int pkt_idx = 0;
    while (pkt_idx < pkt_num) {
        length = e1000_poll(rx_pkt_buf);
        if (length >= 0)
        {
            pkt_lens[pkt_idx] = length;
            rx_pkt_buf += length;
            ++pkt_idx;
        }
        else
        {
            // #define E1000_ICR               0x000C0	/* Interrupt Cause Read - R/clr */
            // #define E1000_IMS               0x000D0	/* Interrupt Mask Set - RW */
            // #define E1000_IMC               0x000D8	/* Interrupt Mask Clear - WO */
            // #define E1000_ICR_RXDMT0  0x00000010	/* rx desc min. threshold (0) */
            // #define E1000_IMS_RXDMT0  E1000_ICR_RXDMT0	/* rx desc min. threshold */
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0); // enable recv ir
            local_flush_dcache();
            WRITE_LOG("put into receive block queue\n")
            do_block(&recv_block_queue, &net_recv_lk);
        }
    }

    // enable receive mask
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0); // enable recv ir
    local_flush_dcache();

    spin_lock_release(&net_recv_lk);

    return pkt_num;  // Bytes it has received
}

#define MAGIC_PROTOCOL_NUM 0x45
#define OP_MASK 0xe000
// #define OP_ACK (1 << 13)
// #define OP_RSD (1 << 14)
// #define OP_DAT (1 << 15)
#define OP_ACK (1 << 10)
#define OP_RSD (1 << 9)
#define OP_DAT (1 << 8)
#define TCP_IP_HEAD_LEN 54

typedef struct stream_space {
    int base;
    int size;
    struct stream_space *next;
} stream_space_t;


static stream_space_t *node_before(stream_space_t *link_head, stream_space_t *link_node)
{
    stream_space_t *link = link_head;
    while (link->next != NULL) // not reached the end
    {
        if (link->next == link_node) // find the link before link_node
            return link;
        link = link->next;
    }
    return NULL; // nothing found (not in the same queue)
}

static void node_remove(stream_space_t *link_head, stream_space_t *link_node)
{
    // remove the space
    stream_space_t *tmp = node_before(link_head, link_node);
    assert(tmp != NULL)
    tmp->next = link_node->next;
    kfree(link_node);
}

// remove corresponding space from given linked list
void occupy_space(stream_space_t *link_head, int offset, int size)
{
    stream_space_t *link = link_head;

    // find the space that contains the offset
    while (link->next != NULL)
    {
        if (link->next->base > offset)
            break;
        link = link->next;
    }

    // link->base <= offset < next->base

    // if the space is occupied
    if (link == link_head || link->base + link->size < offset + size)
    {
        // do nothing
    }

    // link->base <= offset <= offset + size <= link->base + link->size
    else if (size > 0)
    {
        // split the space
        stream_space_t *new_space = kmalloc(sizeof(stream_space_t));
        new_space->base = offset + size;
        new_space->size = link->base + link->size - offset - size;
        new_space->next = link->next;
        link->next = new_space;
        link->size = offset - link->base;

        // check if two nodes are able to be removed
        if (link->size == 0)
            node_remove(link_head, link);
        if (new_space->size == 0)
            node_remove(link_head, new_space);
    }
}

void unoccupy_space_after_ACK(stream_space_t *link_head, int data_size)
{
    stream_space_t *link = link_head->next->next;
    while (link != NULL)
    {
        stream_space_t *tmp = link->next;
        kfree(link);
        link = tmp;
    }
    link_head->next->size = data_size - link_head->next->base; // reset size
    link_head->next->next = NULL;
}

void send_resp_pkt(uint32_t type, int seq, char *tcp_ip_head)
{
    // char tx_buf[8];
    // *(uint16_t *)tx_buf = type; // set operation type
    // *(uint8_t *)tx_buf = MAGIC_PROTOCOL_NUM; // set magic number (cover lower 4 bit of type)
    // *((uint16_t *)tx_buf + 1) = 0; // set length of the packet
    // *((uint32_t *)tx_buf + 1) = seq; // set seq (offset)
    // spin_lock_acquire(&net_send_lk);
    // e1000_transmit(tx_buf, 8);
    // spin_lock_release(&net_send_lk);

    char tx_resp_buf[8 + TCP_IP_HEAD_LEN];
    memcpy((void *)tx_resp_buf, (void *)tcp_ip_head, TCP_IP_HEAD_LEN);
    char *tx_buf = tx_resp_buf + TCP_IP_HEAD_LEN;
    *(short *)tx_buf = type; // set operation type
    *(char *)tx_buf = MAGIC_PROTOCOL_NUM; // set magic number (cover lower 4 bit of type)
    *((uint32_t *)tx_buf + 1) = l2b_endian_w(seq); // set seq (offset)
    spin_lock_acquire(&net_send_lk);
    e1000_transmit(tx_resp_buf, TCP_IP_HEAD_LEN + 8);
    spin_lock_release(&net_send_lk);
}

typedef struct stream_head {
    uint8_t magic, type;
    uint16_t len;
    uint32_t seq;
} stream_head_t;

// receive data by a simple protocol
int do_net_recv_stream(void *buffer, int *nbytes)
{
    WRITE_LOG("into net_recv_stream function\n");
    // nbytes initially stores the max length of data received from this syscall
    int max_bytes = *nbytes;
    bool first_recv = true;

    // wait for the first packet, and get the size of data
    int64_t data_size = -1; // 4 bytes for storing the size.

    // store empty data buffer, mark the space for packets that haven't arrived.
    // use a linked list for that.
    stream_space_t empty_buf_head = {-1, -1};

    // initialize the empty_buf_head
    stream_space_t *new_space = kmalloc(sizeof(stream_space_t));
    new_space->base = 0;
    new_space->size = max_bytes;
    new_space->next = NULL;
    empty_buf_head.next = new_space;

    // int pkt_len_sum = 0;
    char rx_pkt_buf[RX_PKT_SIZE];
    char tcp_ip_head_buf[TCP_IP_HEAD_LEN];
    char *rx_buf = rx_pkt_buf + TCP_IP_HEAD_LEN;

    spin_lock_acquire(&net_recv_lk);

    while (1)
    {
        // try to receive packets
        int len_origin = e1000_poll(rx_pkt_buf);
        int protocol_len = len_origin - TCP_IP_HEAD_LEN;
        if (protocol_len > 8) // 8 bytes for the header, 54 bytes for TCP/IP
        {
            // check the magic number & operation type
            if ((*(char *)rx_buf != MAGIC_PROTOCOL_NUM) || !(*(short *)rx_buf & OP_DAT)) {
                WRITE_LOG("recvstream get a invalid packet!\n");
                continue;
            }

            WRITE_LOG("recvstream get a valid packet!\n");

            // get the seq number (offset)
            uint16_t data_len = b2l_endian_h(*((uint16_t *)rx_buf + 1));
            uint32_t offset = b2l_endian_w(*((uint32_t *)rx_buf + 1));
            // uint32_t offset = *((uint32_t *)rx_buf + 1);

            if (first_recv == true)
            {
                first_recv = false;
                memcpy((void *)tcp_ip_head_buf, (void *)rx_pkt_buf, TCP_IP_HEAD_LEN);
            }

            // if it's the first packet, get the size of data
            if (offset == 0 && data_size == -1)
            {
                data_size = *((uint32_t *)rx_buf + 2);

                // if data is too long, return -1 for error
                if (data_size > max_bytes)
                {
                    *nbytes = -1;
                    spin_lock_release(&net_recv_lk);
                    WRITE_LOG("recv_stream get a buffer from argument that is too small! %d<%d\n", max_bytes, data_size);
                    return -1;
                }

                *nbytes = data_size;

                WRITE_LOG("receive packet: seq:%d len:%d\n", 0, data_len);

                // remove corresponding space from empty_buf_head & copy data to buffer
                occupy_space(&empty_buf_head, 0, data_len);
                memcpy(buffer, (void *)rx_buf + 12, data_len);

                // clear useless space larger than data_size
                occupy_space(&empty_buf_head, data_size, max_bytes - data_size);
            }
            // check legality fo "seq" v.s. "data_size"
            else if (offset + data_len <= data_size || data_size == -1)
            {
                WRITE_LOG("receive packet: seq:%d len:%d\n", offset, data_len);

                // remove corresponding space from empty_buf_head & copy data to buffer
                occupy_space(&empty_buf_head, offset, data_len);
                memcpy(buffer + offset, (void *)rx_buf + 8, data_len);
            }
        }

        // if no packet is received
        else if (len_origin == -1)
        {
            WRITE_LOG("recvstream didn't get a valid packet!\n");

            // try to send a ACK packet to the sender, to inform the received packets
            if (data_size != -1) // only when the first packet has been received
            {
                // send ACK packet
                assert(empty_buf_head.next != NULL)
                send_resp_pkt(OP_ACK, empty_buf_head.next->base, tcp_ip_head_buf);
            }

            my_cpu()->wakeup_time = get_timer() + TIMER_INTERVAL_RECV_STREAM;

            // put into recv block queue
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0); // enable recv ir
            local_flush_dcache();
            WRITE_LOG("put into receive block queue\n")
            do_block(&recv_stream_block_queue, &net_recv_lk);

            // check if new packets have arrived
            local_flush_dcache();
            int new_length = e1000_recv_check();
            // if the first packet hasn't arrived (and no new packets)
            if (new_length == -1 && data_size == -1)
            {
                // send a RSD packet of seq==0
                send_resp_pkt(OP_RSD, 0, tcp_ip_head_buf);
            }
            // if no new packets (with data size known)
            else if (new_length == -1)
            {
                // send a RSD packet, with the seq of the first empty space
                assert(empty_buf_head.next != NULL)
                send_resp_pkt(OP_RSD, empty_buf_head.next->base, tcp_ip_head_buf);
                
                // untrust all the packets after what this function has make sure (send ACK)
                // unoccupy_space_after_ACK(&empty_buf_head, data_size);
            }
        }

        // if all data has been received, leave the loop
        if (data_size != -1 && empty_buf_head.next == NULL)
        {
            WRITE_LOG("send a RSD packet for seq: %d\n", data_size);

            // send a finish packet
            send_resp_pkt(OP_ACK, data_size, tcp_ip_head_buf);
            
            // quit the loop
            break;
        }
    }

    // enable receive mask
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0); // enable recv ir
    local_flush_dcache();

    spin_lock_release(&net_recv_lk);

    return 0;  // Bytes it has received
}


void net_handle_irq(void)
{
    // TODO: [p5-task3] Handle interrupts from network device
    
    // check which interrupt is triggered, send or recv
    local_flush_dcache();
    uint32_t icr = e1000_read_reg(e1000, E1000_ICR);

    // if send interrupt, call e1000_handle_txqe()
    if (icr & E1000_ICR_TXQE)
    {
        e1000_handle_txqe();
    }

    // if recv interrupt, call e1000_handle_rxdmt0()
    if (icr & E1000_ICR_RXDMT0)
    {
        e1000_handle_rxdmt0();
    }
}


void clear_block_queue(list_head *block_queue)
{
    list_node_t *list_free = block_queue->next;
    list_node_t *list_free_next;
    while (list_free != block_queue)
    {
        list_free_next = list_free->next;

        // unblock the process
        list_del(list_free);
        list_add_tail(&ready_queue, list_free);
        pcb_t *proc = find_PCB_addr(list_free);
        proc->status = TASK_READY;

        list_free = list_free_next;
    }
}

void e1000_handle_txqe(void)
{
    // it happends when the send block queue is empty
    // we need to wake up the process that is waiting in do_net_send

    WRITE_LOG("e1000_handle_txqe\n")

    spin_lock_acquire(&net_send_lk);
    spin_lock_acquire(&pcb_lk);

    // if there are processes waiting, wake them up
    if (!list_empty(&send_block_queue))
    {
        // clear the send block queue
        clear_block_queue(&send_block_queue);
    }
    // if not, do nothing

    spin_lock_release(&pcb_lk);
    spin_lock_release(&net_send_lk);

    // disable the interrupt
    e1000_write_reg(e1000, E1000_IMC, E1000_IMS_TXQE);
    local_flush_dcache();
}

void e1000_handle_rxdmt0(void)
{
    // it happends when the send block queue is empty
    // we need to wake up the process that is waiting in do_net_send

    WRITE_LOG("e1000_handle_rxdmt0\n")

    spin_lock_acquire(&net_recv_lk);
    spin_lock_acquire(&pcb_lk);

    // if there are processes waiting, wake them up
    if (!list_empty(&recv_block_queue) || !list_empty(&recv_stream_block_queue))
    {
        // clear the recv block queue
        if (!list_empty(&recv_block_queue))
            clear_block_queue(&recv_block_queue);
        if (!list_empty(&recv_stream_block_queue))
            clear_block_queue(&recv_stream_block_queue);

        // disable the recv interrupt (only when there are processes waiting)
        e1000_write_reg(e1000, E1000_IMC, E1000_IMC_RXDMT0);
        local_flush_dcache();
    }
    // if not, just clear all the packets
    else
    {
        e1000_clear_recv_pkts();
    }

    spin_lock_release(&pcb_lk);
    spin_lock_release(&net_recv_lk);
}

void check_recv_stream()
{
    // check wakeup_time.
    spin_lock_acquire(&pcb_lk);

    list_node_t *proc_node = recv_stream_block_queue.next;
    list_node_t *proc_node_next;

    while (proc_node != &recv_stream_block_queue)
    {
        proc_node_next = proc_node->next;
        pcb_t *proc = find_PCB_addr(proc_node);
        // if reached, then judge the packets, which haven't been received, as time-out
        if (get_timer() >= proc->wakeup_time)
        {
            // unblock the process
            list_del(proc_node);
            list_add_tail(&ready_queue, proc_node);
            proc->status = TASK_READY;
        }
        proc_node = proc_node_next;
    }

    spin_lock_release(&pcb_lk);
}
