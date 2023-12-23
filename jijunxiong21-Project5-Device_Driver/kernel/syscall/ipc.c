// #include <os/lock.h> // ignored
#include <os/sched.h>

barrier_t barriers[BARRIER_NUM];

/* Barrier functions */

void init_barriers(void)
{
    for (int i = 0; i < BARRIER_NUM; i++)
    {
        barriers[i].block_queue.prev = barriers[i].block_queue.next = &(barriers[i].block_queue);
        barriers[i].goal = 0;
        barriers[i].key = 0;
        barriers[i].lock.status = UNLOCKED;
        barriers[i].pid = 0;
        barriers[i].value = 0;
    }
}

int do_barrier_init(int key, int goal)
{
    int barr_idx;

    // one key should only be initialized once
    for (barr_idx = 0; barr_idx < BARRIER_NUM; barr_idx++)
    {
        spin_lock_acquire(&(barriers[barr_idx].lock)); /* into critical section */
        if (barriers[barr_idx].pid == 0)
            break;
        else
            spin_lock_release(&(barriers[barr_idx].lock)); /* out of critical section */
    }
    if (barr_idx == BARRIER_NUM) // if no empty lock is found
    {
        printk("barrier not enough!\n");
        assert(barr_idx == BARRIER_NUM)
    }

    barrier_t *barr = &barriers[barr_idx]; // abbreviated barriers[bar_idx]

    barr->block_queue.prev = barr->block_queue.next = &(barr->block_queue);
    barr->goal = goal - 1;
    barr->key = key;
    barr->pid = my_cpu()->pid;
    barr->value = goal - 1; // initialize the barrier value (goal - 1, for easy comparison with 0)

    spin_lock_release(&(barriers[barr_idx].lock)); /* out of critical section */

    return barr_idx;
}

void do_barrier_wait(int bar_idx)
{
    barrier_t *barr = &barriers[bar_idx];

    spin_lock_acquire(&(barr->lock)); /* into critical section */

    
    // if the barrier value is 0, wake up all the processes in the block queue, for they have all arrived
    if (barr->value == 0)
    {
        while (!list_empty(&(barr->block_queue)))
            do_unblock(barr->block_queue.next);
        barr->value = barr->goal; // reset the barrier value
    }
    else
    {
        barr->value -= 1; // decrease the barrier value, for one process has arrived
        do_block(&(barr->block_queue), &(barr->lock)); /* inside critical section change */
    }

    spin_lock_release(&(barr->lock)); /* out of critical section */
}

void do_barrier_destroy(int bar_idx)
{
    barrier_t *barr = &barriers[bar_idx];

    spin_lock_acquire(&(barr->lock)); /* into critical section */

    while (!list_empty(&(barr->block_queue)))
        do_unblock(barr->block_queue.next);
    // barr->block_queue.prev = barr->block_queue.next = &(barr->block_queue);
    barr->goal = 0;
    barr->key = 0;
    barr->lock.status = UNLOCKED;
    barr->pid = 0;
    barr->value = 0;

    spin_lock_release(&(barr->lock)); /* out of critical section */
}

/* Semaphore functions */

semaphore_t semaphores[SEMAPHORE_NUM];

void init_semaphores(void)
{
    for (int i = 0; i < SEMAPHORE_NUM; i++)
    {
        semaphores[i].block_queue.prev = semaphores[i].block_queue.next = &(semaphores[i].block_queue);
        semaphores[i].key = 0;
        semaphores[i].lock.status = UNLOCKED;
        semaphores[i].pid = 0;
        semaphores[i].value = 0;
    }
}

int do_semaphore_init(int key, int init)
{
    int sema_idx;

    // one key should only be initialized once
    for (sema_idx = 0; sema_idx < SEMAPHORE_NUM; sema_idx++)
    {
        spin_lock_acquire(&(semaphores[sema_idx].lock)); /* into critical section */
        if (semaphores[sema_idx].pid == 0)
            break;
        else
            spin_lock_release(&(semaphores[sema_idx].lock)); /* out of critical section */
    }
    if (sema_idx == SEMAPHORE_NUM) // if no empty lock is found
    {
        printk("semaphore not enough!\n");
        assert(sema_idx == SEMAPHORE_NUM)
    }

    semaphore_t *sema = &semaphores[sema_idx]; // abbreviated semaphores[sema_idx]

    sema->block_queue.prev = sema->block_queue.next = &(sema->block_queue);
    sema->key = key;
    sema->pid = my_cpu()->pid;
    sema->value = init; // initialize the semaphore value

    spin_lock_release(&(semaphores[sema_idx].lock)); /* out of critical section */

    return sema_idx;
}

void do_semaphore_up(int sema_idx)
{
    semaphore_t *sema = &semaphores[sema_idx];

    spin_lock_acquire(&(sema->lock)); /* into critical section */

    // if some process is in the block queue, wake them up
    if (!list_empty(&(sema->block_queue)))
        do_unblock(sema->block_queue.next);
    else
        sema->value += 1;

    spin_lock_release(&(sema->lock)); /* out of critical section */
}

void do_semaphore_down(int sema_idx)
{
    semaphore_t *sema = &semaphores[sema_idx];

    spin_lock_acquire(&(sema->lock)); /* into critical section */

    if (sema->value > 0)
        sema->value -= 1;
    else
        do_block(&(sema->block_queue), &(sema->lock)); /* inside critical section change */

    spin_lock_release(&(sema->lock)); /* out of critical section */
}

void do_semaphore_destroy(int sema_idx)
{
    semaphore_t *sema = &semaphores[sema_idx];

    spin_lock_acquire(&(sema->lock)); /* into critical section */

    while (!list_empty(&(sema->block_queue)))
        do_unblock(sema->block_queue.next);
    // sema->block_queue.prev = sema->block_queue.next = &(sema->block_queue);
    sema->key = 0;
    sema->lock.status = UNLOCKED;
    sema->pid = 0;
    sema->value = 0;

    spin_lock_release(&(sema->lock)); /* out of critical section */
}

/* Mailbox functions */

mailbox_t mailboxes[MBOX_NUM];

void init_mbox()
{
    for (int i = 0; i < MBOX_NUM; i++)
    {
        mailboxes[i].apply_times = 0;
        mailboxes[i].recv_queue.prev = mailboxes[i].recv_queue.next = &(mailboxes[i].recv_queue);
        mailboxes[i].send_queue.prev = mailboxes[i].send_queue.next = &(mailboxes[i].send_queue);
        mailboxes[i].data[0] = '\0';
        mailboxes[i].begin = 0;
        mailboxes[i].end = 0;
        mailboxes[i].lock.status = UNLOCKED;
        mailboxes[i].name[0] = '\0';
    }
}

int do_mbox_open(char *name)
{
    int mbox_idx;

    // one key should only be initialized once
    for (mbox_idx = 0; mbox_idx < MBOX_NUM; mbox_idx++)
    {
        spin_lock_acquire(&(mailboxes[mbox_idx].lock)); /* into critical section */
        if (strncmp(name, mailboxes[mbox_idx].name, MAX_MBOX_NAME_LENGTH) == 0)
            break;
        else
            spin_lock_release(&(mailboxes[mbox_idx].lock)); /* out of critical section */
    }
    if (mbox_idx == MBOX_NUM) // if no matched key is found
    {
        for (mbox_idx = 0; mbox_idx < MBOX_NUM; mbox_idx++)
        {
            spin_lock_acquire(&(mailboxes[mbox_idx].lock)); /* into critical section */
            if (mailboxes[mbox_idx].apply_times == 0)
                break;
            else
                spin_lock_release(&(mailboxes[mbox_idx].lock)); /* out of critical section */
        }
    }
    if (mbox_idx == MBOX_NUM) // if no empty lock is found
    {
        printk("mailbox not enough!\n");
        assert(mbox_idx == MBOX_NUM)
    }

    mailbox_t *mbox = &mailboxes[mbox_idx]; // abbreviated mailboxes[mbox_idx]

    if (mbox->apply_times == 0)
    {
        // mbox->recv_queue.prev = mbox->recv_queue.next = &(mbox->recv_queue);
        // mbox->send_queue.prev = mbox->send_queue.next = &(mbox->send_queue);
        // mbox->data[0] = '\0';
        // mbox->begin = 0;
        // mbox->end = 0;
        strncpy(mbox->name, name, MAX_MBOX_NAME_LENGTH);
    }

    mbox->apply_times += 1;

    // spin_lock_acquire(&pcb_lk); /* enter critical section */
    uint64_t mbox_ocp_tmp = (uint64_t)1 << mbox_idx;
    atomic_or_d(mbox_ocp_tmp, (ptr_t)&(current_running[get_current_cpu_id()]->mbox_ocp));
    // current_running[get_current_cpu_id()]->mbox_ocp |= ((uint64_t)1 << mbox_idx);
    // spin_lock_release(&pcb_lk); /* leave critical section */

    spin_lock_release(&(mailboxes[mbox_idx].lock)); /* out of critical section */

    return mbox_idx;
}

void do_mbox_close(int mbox_idx)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];

    spin_lock_acquire(&(mbox->lock)); /* into critical section */

    if (mbox->apply_times > 0)
        mbox->apply_times -= 1;
    else
    {
        mbox->recv_queue.prev = mbox->recv_queue.next = &(mbox->recv_queue);
        mbox->send_queue.prev = mbox->send_queue.next = &(mbox->send_queue);
        mbox->data[0] = '\0';
        mbox->begin = 0;
        mbox->end = 0;
        mbox->name[0] = '\0';
    }

    spin_lock_release(&(mbox->lock)); /* out of critical section */
}

// copy the message to the mailbox
void data_in_mbox(void *msg, char *mbox_data, int mbox_begin, int mbox_end, int msg_length)
{
    // if end + msg_length
    if (mbox_end + msg_length <= MAX_MBOX_LENGTH)
    {
        memcpy((void *)(mbox_data) + mbox_end, msg, msg_length);
    }
    else
    {
        memcpy((void *)(mbox_data) + mbox_end, msg, MAX_MBOX_LENGTH - mbox_end);
        memcpy((void *)(mbox_data), msg + MAX_MBOX_LENGTH - mbox_end, msg_length - MAX_MBOX_LENGTH + mbox_end);
    }
}

// send message to mailbox
int do_mbox_send(int mbox_idx, void * msg, int msg_length)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];
    int block_num = 0;
    int mbox_avail_len;

    // check whether the msg_length is too long
    if (msg_length > MAX_MBOX_LENGTH)
        return -1;

    spin_lock_acquire(&(mbox->lock)); /* into critical section */

    // check if the message is too long
    while (mbox_avail_len = (mbox->begin <= mbox->end
                                 ? mbox->begin - mbox->end - 1 + MAX_MBOX_LENGTH
                                 : mbox->begin - mbox->end - 1),
           msg_length > mbox_avail_len) {
        block_num += 1;
        // data_in_mbox(msg, mbox->data, mbox->begin, mbox->end, mbox_avail_len);
        // msg_length -= mbox_avail_len;
        do_block(&(mbox->send_queue),
                 &(mbox->lock)); /* inside critical section change */
    }

    data_in_mbox(msg, mbox->data, mbox->begin, mbox->end, msg_length);

    mbox->end = (mbox->end + msg_length) % MAX_MBOX_LENGTH;

    // if some process is in the block queue, wake them up (to check if it can get the message)
    while (!list_empty(&(mbox->recv_queue)))
        do_unblock(mbox->recv_queue.next);
    // while (!list_empty(&(mbox->send_queue)))
    //     do_unblock(mbox->send_queue.next);

    spin_lock_release(&(mbox->lock)); /* out of critical section */

    return block_num;
}

// copy the message from the mailbox
void data_out_mbox(void *msg, char *mbox_data, int mbox_begin, int mbox_end, int msg_length)
{
    // copy the message from the mailbox
    if (mbox_begin + msg_length <= MAX_MBOX_LENGTH)
    {
        memcpy(msg, (void *)(mbox_data) + mbox_begin, msg_length);
    }
    else
    {
        memcpy(msg, (void *)(mbox_data) + mbox_begin, MAX_MBOX_LENGTH - mbox_begin);
        memcpy(msg + MAX_MBOX_LENGTH - mbox_begin, (void *)(mbox_data), msg_length - MAX_MBOX_LENGTH + mbox_begin);
    }
}

// receive message from the mailbox
int do_mbox_recv(int mbox_idx, void * msg, int msg_length)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];
    int block_num = 0;
    int mbox_avail_len;

    // check whether the msg_length is too long
    if (msg_length > MAX_MBOX_LENGTH)
        return -1;

    spin_lock_acquire(&(mbox->lock)); /* into critical section */

    // check if data is enough
    while (mbox_avail_len = (mbox->begin <= mbox->end
                             ? mbox->end - mbox->begin
                             : mbox->end - mbox->begin + MAX_MBOX_LENGTH)
           , msg_length > mbox_avail_len) {
        block_num += 1;
        // data_out_mbox(msg, mbox->data, mbox->begin, mbox->end, mbox_avail_len);
        // msg_length -= mbox_avail_len;
        do_block(&(mbox->recv_queue), &(mbox->lock)); /* inside critical section change */
    }

    data_out_mbox(msg, mbox->data, mbox->begin, mbox->end, msg_length);

    mbox->begin = (mbox->begin + msg_length) % MAX_MBOX_LENGTH;

    // if some process is in the block queue, wake them up (to check if it can get the message)
    // while (!list_empty(&(mbox->recv_queue)))
    //     do_unblock(mbox->recv_queue.next);
    while (!list_empty(&(mbox->send_queue)))
        do_unblock(mbox->send_queue.next);

    spin_lock_release(&(mbox->lock)); /* out of critical section */

    return block_num;
}

void do_mbox_destroy(int mbox_idx)
{
    mailbox_t *mbox = &mailboxes[mbox_idx];

    spin_lock_acquire(&(mbox->lock)); /* into critical section */

    mbox->apply_times = 0;
    while (!list_empty(&(mbox->recv_queue)))
        do_unblock(mbox->recv_queue.next);
    while (!list_empty(&(mbox->send_queue)))
        do_unblock(mbox->send_queue.next);
    // mbox->recv_queue.prev = mbox->recv_queue.next = &(mbox->recv_queue);
    // mbox->send_queue.prev = mbox->send_queue.next = &(mbox->send_queue);
    mbox->data[0] = '\0';
    mbox->begin = 0;
    mbox->end = 0;
    mbox->name[0] = '\0';

    spin_lock_release(&(mbox->lock)); /* out of critical section */
}
