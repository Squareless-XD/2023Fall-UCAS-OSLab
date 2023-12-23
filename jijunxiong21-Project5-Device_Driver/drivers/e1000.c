#include <e1000.h>
// #include <type.h>
// #include <os/string.h>
#include <os/time.h>
// #include <assert.h>
// #include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    // e1000_write_reg(e1000, E1000_TDH, 0);
    // e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    // e1000_write_reg(e1000, E1000_RDH, 0);
    // e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    local_flush_dcache(); // after write and before read

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for (int i = 0; i < TXDESCS; ++i) {
        tx_desc_array[i].addr = kva2pa((ptr_t)tx_pkt_buffer[i]);
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS; // DEXT==0: legacy mode
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
        tx_desc_array[i].css = 0;
        tx_desc_array[i].special = 0;
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    // #define E1000_TDBAL             0x03800	/* TX Descriptor Base Address Low - RW */
    // #define E1000_TDBAH             0x03804	/* TX Descriptor Base Address High - RW */
    // #define E1000_TDLEN             0x03808	/* TX Descriptor Length - RW */
    // initialize tx descriptors: these above
    e1000_write_reg(e1000, E1000_TDBAL,
                    kva2pa((ptr_t)tx_desc_array) & 0xffffffff);
    e1000_write_reg(e1000, E1000_TDBAH,
                    (kva2pa((ptr_t)tx_desc_array) >> 32) & 0xffffffff);
    e1000_write_reg(e1000, E1000_TDLEN, sizeof(struct e1000_tx_desc) * TXDESCS);

    /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    // #define E1000_TCTL              0x00400	/* TX Control - RW */
    // #define E1000_TCTL_EN           0x00000002    /* enable tx */
    // #define E1000_TCTL_PSP          0x00000008    /* pad short packets */
    // #define E1000_TCTL_CT           0x00000ff0    /* collision threshold */
    // #define E1000_TCTL_COLD         0x003ff000    /* collision distance */
    // initialize TCTL register:
    // set TCTL.EN as 1, TCTL.PSP as 1、TCTL.CT as 0x10H、TCTL.COLD as 0x40H
    e1000_write_reg(e1000, E1000_TCTL,
                    E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << 4) |
                        (0x40 << 12));

    e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);

    local_flush_dcache(); // after write
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAL[0]/RAH[0] */
    // #define E1000_RA                0x05400	/* Receive Address - RW Array */
    e1000_write_reg_array(e1000, E1000_RA, 0, *(uint32_t *)enetaddr);
    e1000_write_reg_array(e1000, E1000_RA, 1,
                          *(uint16_t *)(enetaddr + 4) | E1000_RAH_AV);

    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i = 0; i < RXDESCS; ++i) {
        rx_desc_array[i].addr = kva2pa((ptr_t)rx_pkt_buffer[i]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0; // not finished
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    // #define E1000_RDBAL             0x02800	/* RX Descriptor Base Address Low - RW */
    // #define E1000_RDBAH             0x02804	/* RX Descriptor Base Address High - RW */
    // #define E1000_RDLEN             0x02808	/* RX Descriptor Length - RW */
    // initialize rx descriptors: these above
    e1000_write_reg(e1000, E1000_RDBAL,
                    kva2pa((ptr_t)rx_desc_array) & 0xffffffff);
    e1000_write_reg(e1000, E1000_RDBAH,
                    (kva2pa((ptr_t)rx_desc_array) >> 32) & 0xffffffff);
    e1000_write_reg(e1000, E1000_RDLEN, sizeof(struct e1000_rx_desc) * RXDESCS);

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);

    /* TODO: [p5-task2] Program the Receive Control Register */
    // #define E1000_RCTL              0x00100	/* RX Control - RW */
    // #define E1000_RCTL_EN		    0x00000002	/* enable */
    // #define E1000_RCTL_BAM		    0x00008000	/* broadcast enable */
    // #define E1000_RCTL_BSEX		    0x02000000	/* Buffer size extension */
    // #define E1000_RCTL_SZ_2048	    0x00000000	/* rx buffer size 2048 */
    e1000_write_reg(e1000, E1000_RCTL,
                    E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048);
    // E1000_RCTL_BSEX==0

    /* TODO: [p5-task3] Enable RXDMT0 Interrupt */
    // #define E1000_RCTL_RDMTS_HALF	0x00000000	/* rx desc min threshold size */
    // #define E1000_ICS               0x000C8	/* Interrupt Cause Set - WO */
    // #define E1000_ICR_RXDMT0  0x00000010	/* rx desc min. threshold (0) */
    // #define E1000_ICS_RXDMT0  E1000_ICR_RXDMT0	/* rx desc min. threshold */
    // e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_RDMTS_HALF); // write 0, ignored
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);

    local_flush_dcache(); // after write
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length) // with net_send_lk locked
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    /*
     * get current TDT (transmit descriptor tail index)
     * find the descriptor and corresponding buffer
     */

    assert(length <= TX_PKT_SIZE)

    local_flush_dcache(); // before read

    uint64_t tdt_idx = e1000_read_reg(e1000, E1000_TDT);
    struct e1000_tx_desc *tx_desc = &tx_desc_array[tdt_idx];

    /*
     * check if the descriptor is available.
     * if not, return -1
     */
    if (!(tx_desc->status & E1000_TXD_STAT_DD))
        return -1;

    /*
     * set up the descriptor
     */
    char *tx_buffer = tx_pkt_buffer[tdt_idx];
    // use the corresponding buffer to store the packet
    // tx_desc->addr = kva2pa((uint64_t)tx_buffer);
    memcpy((uint8_t *)tx_buffer, txpacket, length);

    // length
    tx_desc->length = length;

    // clear DD bit: not finished
    tx_desc->status &= ~E1000_TXD_STAT_DD;

    // set EOP and RS bit: end of packet and legacy mode
    tx_desc->cmd |= E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

    /*
     * update TDT
     */
    e1000_write_reg(e1000, E1000_TDT, (tdt_idx + 1) % TXDESCS); // after write

    local_flush_dcache();

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer) // with net_recv_lk locked
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    /*
     * get current TDT (transmit descriptor tail index)
     * find the descriptor and corresponding buffer
     */

    local_flush_dcache(); // before read

    uint64_t rdt_idx = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS; // next
    struct e1000_rx_desc *rx_desc = &rx_desc_array[rdt_idx];

    /*
     * check if the descriptor is finished.
     * if not, return -1
     */
    if (!(rx_desc->status & E1000_RXD_STAT_DD))
        return -1;

    /*
     * set up the descriptor
     */
    char *rx_buffer = rx_pkt_buffer[rdt_idx];
    int length = rx_desc->length;
    // use the corresponding buffer to store the packet
    // rx_desc->addr = kva2pa((ptr_t)rx_buffer);
    memcpy(rxbuffer, (uint8_t *)rx_buffer, length);

    // clear
    rx_desc->length = 0;
    rx_desc->csum = 0;
    rx_desc->status = 0; // not finished
    rx_desc->errors = 0;
    rx_desc->special = 0;

    /*
     * update TDT
     */
    e1000_write_reg(e1000, E1000_RDT, rdt_idx); // after write

    local_flush_dcache();

    return length;
}

// check whether there is a packet in the receive queue
int e1000_recv_check(void)
{
    local_flush_dcache(); // before read

    uint64_t rdt_idx = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS; // next
    struct e1000_rx_desc *rx_desc = &rx_desc_array[rdt_idx];

    /*
     * check if the descriptor is finished.
     * if not, return -1
     */
    if (!(rx_desc->status & E1000_RXD_STAT_DD))
        return -1;

    /*
     * set up the descriptor
     */
    int length = rx_desc->length;

    return length;
}

void e1000_clear_recv_pkts(void) // with net_recv_lk locked
{
    local_flush_dcache(); // before read
    uint64_t rdt_idx = e1000_read_reg(e1000, E1000_RDT) + 1;

    // clear all the packets without storing them
    int i;
    for (i = 1; i < RXDESCS; ++i) // NOTE: one descriptor is reserved since head!=tail
    {
        // find the struct to check
        struct e1000_rx_desc *rx_desc = &rx_desc_array[(rdt_idx + i) % RXDESCS];

        /*
         * check if the descriptor is finished.
         * if not, leave the loop
         */
        if (!(rx_desc->status & E1000_RXD_STAT_DD))
            break;

        /*
         * if so, clear the descriptor without storing the packet
         */
        // rx_desc->addr = kva2pa((ptr_t)rx_buffer);
        rx_desc->length = 0;
        rx_desc->csum = 0;
        rx_desc->status = 0; // not finished
        rx_desc->errors = 0;
        rx_desc->special = 0;
    }

    /*
    * update TDT to the last finished descriptor
    */
    e1000_write_reg(e1000, E1000_RDT, (rdt_idx + i - 1) % RXDESCS); // after write
    local_flush_dcache();
}
