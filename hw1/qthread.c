/*
 * file:        qthread.c
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Spring 2018
 */

/* a bunch of includes which will be useful */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include "qthread.h"

void *main_stack;          // main stack pointer.
qthread_t current;         // current stack pointer.
struct tqueue active;      // active thread queue.
struct tqueue sleepers;    // sleeping thread queue.
struct tqueue io_waiters;  // queue of threads waiting for I/O.

/* prototypes for stack.c and switch.s */
extern void switch_to(void **location_for_old_sp, void *new_value);
extern void *setup_stack(int *stack, void *func, void *arg1, void *arg2);

/**
 * Qthread structure 
 */
struct qthread {
    void     *sp;     // stack pointer
    void     *stack;  // initial memory address of thread stack
    qthread_t next;   // next thread
    void     *retval; // return value
    qthread_t waiter; // pointer to thread that waits for this thread
    bool      done;   // done flag
    io_status status; // io status
    int       fd;     // file descriptor
}; 

/**
 * Pop the thread from the thread queue.
 *
 * @param tq the thread queue.
 */
static qthread_t tq_pop(tqueue_t tq) {
    if (tq == NULL || tq->head == NULL) {
        return NULL;
    } else {
        qthread_t qt = tq->head;
        tq->head = qt->next;
        if (tq->head == NULL) {
            tq->tail = NULL;
        }
        return qt;
    }
}

/**
 * Append the thread to the thread queue.
 *
 * @param tq the thread queue.
 * @param qt thread pointer.
 */
static void tq_append(tqueue_t tq, qthread_t qt) {
    if (tq == NULL || qt == NULL) {
        return;
    }
    qt->next = NULL;
    if (tq->head == NULL) {
        tq->head = tq->tail = qt;
    } else {
        tq->tail->next = qt;
        tq->tail = qt;
    }
}

/**
 * Check whether the thread queue is empty.
 *
 * @param tq the thread queue.
 * @return true if the thread queue is empty and otherwise.
 */
static bool tq_empty(tqueue_t tq) {
    return tq == NULL ? true : tq->head == NULL;
}

/**
 * Wait mathod for I/O
 */
static void io_wait() {
    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    qthread_t curr = io_waiters.head;
    while (curr != NULL) {
        if (curr->status == write_mode) {
            FD_SET(curr->fd, &wfds);
        } else if (curr->status == read_mode) {
            FD_SET(curr->fd, &rfds);
        }
        curr = curr->next;
    }
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = PEND_TIME
    };
    select(FD_SETSIZE, &rfds, &wfds, NULL, &tv);
    struct tqueue tmp;
    while (!tq_empty(&io_waiters)) {
        qthread_t curr = tq_pop(&io_waiters);
        if (FD_ISSET(curr->fd, &rfds) || FD_ISSET(curr->fd, &wfds)) {
            tq_append(&active, curr);
        } else {
            tq_append(&tmp, curr);
        }
    }
    while (!tq_empty(&tmp)) {
        tq_append(&io_waiters, tq_pop(&tmp));
    }
}

/**
 * Schedule current active thread.
 *
 * @param save_location previous stack pointer to save.
 */
static void schedule(void *save_location) {
    qthread_t self = current;
again:
    current = tq_pop(&active);
    if (current == self) {
        return;
    }
    if (current == NULL) {
        if (tq_empty(&sleepers) && tq_empty(&io_waiters)) {
            switch_to(NULL, main_stack);
        } 
        if (!tq_empty(&sleepers)) {
            usleep(PEND_TIME);
            while (!tq_empty(&sleepers)) {
                tq_append(&active, tq_pop(&sleepers));
            }
            goto again;
        } 
        if (!tq_empty(&io_waiters)) {
            io_wait();
            goto again;
        } 
    }
    switch_to(save_location, current->sp);
}

/**
 * Tell time of now.
 */
static unsigned get_usecs(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/**
 * Jump function for qthread_create to qthread_exit and return exit value.
 *
 * @param f function pointer which has one argument and a return value
 * @param arg1 argument of the function pointer
 */
static void create_run(f_1arg_t f, void *arg1) {
    qthread_exit(f(arg1));
}

/**
 * Start a thread of callback function f with two arguments
 * (function passed to qthread_start is not allowed to return)
 *
 * @param f function which has two arguments and no return value
 * @param arg1 first argument of the function 
 * @param arg2 second argument of the function 
 */
qthread_t qthread_start(f_2arg_t f, void *arg1, void *arg2){
    qthread_t qt = malloc(sizeof(*qt));
    qt->stack    = malloc(STACK_SIZE);
    qt->sp       = setup_stack(qt->stack + STACK_SIZE, f, arg1, arg2);
    qt->next     = NULL;
    qt->retval   = NULL;
    qt->waiter   = NULL;
    qt->done     = false;
    qt->status   = no_io;
    qt->fd       = -1;
    tq_append(&active, qt);
    return qt;
}

/**
 * Create a thread of callback function f with two arguments
 * (function passed to qthread_create is allowed to return)
 *
 * @param f function which has one argument and a return value
 * @param arg1 argument of the function 
 */
qthread_t qthread_create(f_1arg_t f, void *arg1){
    return qthread_start((f_2arg_t) create_run, f, arg1);
}

/**
 * Run until the last thread exits.
 */
void qthread_run(void) {
    schedule(&main_stack);
}

/**
 * Yield to the next runnable thread.
 */
void qthread_yield(void){
    tq_append(&active, current);
    schedule(&current->sp);
}

/**
 * Exit a thread with a return value.
 *
 * @param val return value;
 */
void qthread_exit(void *val){
    qthread_t qt = current;
    qt->retval = val;
    qt->done = true;
    if (qt->waiter) {
        tq_append(&active, qt->waiter);
        qt->waiter = NULL;
    }
    schedule(&current->sp);
}

/**
 * Exit argument is returned by qthread_join. 
 * Note that join blocks if thread hasn't exited yet,
 * and may crash if thread doesn't exist.
 *
 * @param qt the thread for current thread to join.
 * @return thread exit value.
 */
void *qthread_join(qthread_t qt){
    if (!qt->done) {
        qt->waiter = current;
        schedule(&current->sp);
    } 
    void *val = qt->retval;
    free(qt);
    return val;
}

/**
 * Yield to next runnable thread, making arrangements
 * to be put back on the active list after 'usecs' timeout. 
 *
 * @param usecs time to sleep
 */
void qthread_usleep(long int usecs){
    unsigned wakeup = get_usecs() + usecs;
    while (get_usecs() < wakeup) {
        tq_append(&sleepers, current);
        schedule(&current->sp);
    }
}

/**
 * Initiate the mutex.
 *
 * @param mutex mutex pointer
 */
void qthread_mutex_init(qthread_mutex_t *mutex){
    mutex->locked = false;
    mutex->waiters.tail = NULL;
    mutex->waiters.head = NULL;
}

/**
 * Lock the mutex.
 *
 * @param mutex mutex pointer
 */
void qthread_mutex_lock(qthread_mutex_t *mutex){
    if (mutex == NULL) {
        return;
    }
    if (!mutex->locked) {
        mutex->locked = true;
    } else {
        tq_append(&mutex->waiters, current);
        schedule(&current->sp);
    }
}

/**
 * Unlock the mutex.
 *
 * @param mutex mutex pointer
 */
void qthread_mutex_unlock(qthread_mutex_t *mutex){
    if (mutex == NULL) {
        return;
    }
    if (tq_empty(&mutex->waiters)) {
        mutex->locked = false;
    } else {
        tq_append(&active, tq_pop(&mutex->waiters));
    }
}

/**
 * Initiate the condition variable.
 *
 * @param cond condition variable pointer
 */
void qthread_cond_init(qthread_cond_t *cond){
    cond->waiters.head = NULL;
    cond->waiters.tail = NULL;
}

/**
 * Wait the condition variable.
 *
 * @param cond condition variable pointer
 * @param mutex mutex pointer
 */
void qthread_cond_wait(qthread_cond_t *cond, qthread_mutex_t *mutex){
    if (cond == NULL || mutex == NULL) {
        return;
    }
    tq_append(&cond->waiters, current);
    qthread_mutex_unlock(mutex);
    schedule(&current->sp);
    qthread_mutex_lock(mutex);
}

/**
 * Signal the condition variable.
 *
 * @param cond condition variable pointer
 */
void qthread_cond_signal(qthread_cond_t *cond){
    if (cond == NULL) {
        return;
    }
    if (!tq_empty(&cond->waiters)) {
        tq_append(&active, tq_pop(&cond->waiters));
    }
}

/**
 * Signal all the condition variable.
 *
 * @param cond condition variable pointer
 */
void qthread_cond_broadcast(qthread_cond_t *cond){
    if (cond == NULL) {
        return;
    }
    while (!tq_empty(&cond->waiters)) {
        tq_append(&active, tq_pop(&cond->waiters));
    }
}

// I/O related functions

/**
 * Thread read function.
 *
 * If there are no runnable threads, your scheduler needs to block
 * waiting for one of these blocking functions to return. You should
 * probably do this using the select() system call, indicating all the
 * file descriptors that threads are blocked on, and with a timeout
 * for the earliest thread waiting in qthread_usleep()
 *
 * make sure that the file descriptor is in non-blocking mode, try to
 * read from it, if you get -1 / EAGAIN then add it to the list of
 * file descriptors to go in the big scheduling 'select()' and switch
 * to another thread.
 *
 * @param fd file descriptor
 * @param buf reading buffer
 * @param len length of reading
 * @return length of actual reading.
 */
ssize_t qthread_read(int fd, void *buf, size_t len){
    // set non-blocking mode every time. 
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
    current->status = read_mode;
    current->fd = fd;
    while ((val = read(fd, buf, len)) == -1 && errno == EAGAIN) {
        tq_append(&io_waiters, current);
        schedule(&current->sp);
    }
    return val;
}

/**
 * Thread receive function for socket.
 *
 * @param fd file descriptor of socket
 * @param buf reading buffer
 * @param len length of reading
 * @param flags receiving mode
 * @return length of receiving size.
 */
ssize_t qthread_recv(int sockfd, void *buf, size_t len, int flags){
    return qthread_read(sockfd, buf, len);
}

/* like read - make sure the descriptor is in non-blocking mode, check
 * if if there's anything there - if so, return it, otherwise save fd
 * and switch to another thread. Note that accept() counts as a 'read'
 * for the select call.
 */
int qthread_accept(int fd, struct sockaddr *addr, socklen_t *addrlen){
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
    current->status = read_mode;
    current->fd = fd;
    while ((val = accept(fd, addr, addrlen)) == -1 && errno == EAGAIN) {
        tq_append(&io_waiters, current);
        schedule(&current->sp);
    }
    return val;
}

/**
 * Thread write function.
 *
 * Like read, again. Note that this is an output, rather than an input
 * - it can block if the network is slow, although it's not likely to
 * in most of our testing.
 *
 * @param fd file descriptor
 * @param buf writing buffer
 * @param len length of writing
* @return length of actual writing.
 */
ssize_t qthread_write(int fd, void *buf, size_t len){
    int val, tmp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
    current->status = write_mode;
    current->fd = fd;
    while ((val = write(fd, buf, len)) == -1 && errno == EAGAIN) {
        tq_append(&io_waiters, current);
        schedule(&current->sp);
    }
    return val;
}

/**
 * Thread send function for socket.
 *
 * @param fd file descriptor of socket
 * @param buf writing buffer
 * @param len length of writing
 * @param flags sending mode
 * @return length of sending size.
 */
ssize_t qthread_send(int fd, void *buf, size_t len, int flags){
    return qthread_write(fd, buf, len);
}
