/*
 * file:        qthread.h
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Spring 2018
 */
#ifndef __QTHREAD_H__
#define __QTHREAD_H__

#ifndef STACK_SIZE
#define STACK_SIZE 8192
#endif

#ifndef PEND_TIME
#define PEND_TIME 10000
#endif

#include <sys/socket.h>

// boolean value
typedef enum {false, true} bool;
// I/O status of thread
typedef enum {no_io, read_mode, write_mode} io_status; 
// function pointer which has two arguments
typedef void (*f_2arg_t) (void *, void *);
// function pointer which has one argument and a return value 
typedef void *(*f_1arg_t) (void *);

/**
 * Qthread structure 
 */
struct qthread;
typedef struct qthread *qthread_t; 

/**
 * thread queue structure 
 */
struct tqueue {
    qthread_t head, tail;
};
typedef struct tqueue *tqueue_t;

/**
 * Qthread mutex structure 
 */
struct qthread_mutex {
    bool          locked;
    struct tqueue waiters;
};
typedef struct qthread_mutex qthread_mutex_t;

/**
 * Qthread condition structure 
 */
struct qthread_cond {
    struct tqueue waiters;
};
typedef struct qthread_cond qthread_cond_t;

// Qthread functions: qthread_start/create/run/yield/exit/join/usleep

/**
 * Run until the last thread exits
 */
void qthread_run(void);

/**
 * Start a thread of callback function f with two arguments
 * (function passed to qthread_start is not allowed to return)
 *
 * @param f function which has two arguments and no return value
 * @param arg1 first argument of the function 
 * @param arg2 second argument of the function 
 */
qthread_t qthread_start(f_2arg_t f, void *arg1, void *arg2);

/**
 * Create a thread of callback function f with two arguments
 * (function passed to qthread_create is allowed to return)
 *
 * @param f function which has one arguments and a return value
 * @param arg1 argument of the function 
 */
qthread_t qthread_create(f_1arg_t f, void *arg1);

/**
 * Yield to the next runnable thread.
 */
void qthread_yield(void);

/**
 * Exit a thread with a return value.
 *
 * @param val return value;
 */
void qthread_exit(void *val);

/**
 * Exit argument is returned by qthread_join. 
 * Note that join blocks if thread hasn't exited yet,
 * and may crash if thread doesn't exist.
 *
 * @param qt the thread for current thread to join.
 * @return thread exit value.
 */
void *qthread_join(qthread_t thread);

/**
 * Yield to next runnable thread, making arrangements
 * to be put back on the active list after 'usecs' timeout. 
 *
 * @param usecs time to sleep
 */
void qthread_usleep(long int usecs);

// Mutex functions: qthread_mutex_init/lock/unlock

/**
 * Initiate the mutex.
 *
 * @param mutex mutex pointer
 */
void qthread_mutex_init(qthread_mutex_t *mutex);

/**
 * Lock the mutex.
 *
 * @param mutex mutex pointer
 */
void qthread_mutex_lock(qthread_mutex_t *mutex);

/**
 * Unlock the mutex.
 *
 * @param mutex mutex pointer
 */
void qthread_mutex_unlock(qthread_mutex_t *mutex);

// Condition functions: qthread_cond_init/wait/signal/broadcast

/**
 * Initiate the condition variable.
 *
 * @param cond condition variable pointer
 */
void qthread_cond_init(qthread_cond_t *cond);

/**
 * Wait the condition variable.
 *
 * @param cond condition variable pointer
 * @param mutex mutex pointer
 */
void qthread_cond_wait(qthread_cond_t *cond, qthread_mutex_t *mutex);

/**
 * Signal the condition variable.
 *
 * @param cond condition variable pointer
 */
void qthread_cond_signal(qthread_cond_t *cond);

/**
 * Signal all the condition variable.
 *
 * @param cond condition variable pointer
 */
void qthread_cond_broadcast(qthread_cond_t *cond);

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
ssize_t qthread_read(int sockfd, void *buf, size_t len);

/**
 * Thread receive function for socket.
 *
 * @param fd file descriptor of socket
 * @param buf reading buffer
 * @param len length of reading
 * @param flags receiving mode
 * @return length of receiving size.
 */
ssize_t qthread_recv(int sockfd, void *buf, size_t len, int flags);

int qthread_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

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
ssize_t qthread_write(int sockfd, void *buf, size_t len);

/**
 * Thread send function for socket.
 *
 * @param fd file descriptor of socket
 * @param buf writing buffer
 * @param len length of writing
 * @param flags sending mode
 * @return length of sending size.
 */
ssize_t qthread_send(int sockfd, void *buf, size_t len, int flags);

#endif
