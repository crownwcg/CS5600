/*
 * file:        misc.c
 * description: Support functions for Homework 2
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, Oct. 2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <assert.h>

void q2(void);
void q3(void);

int    end_time = 0;
double speedup = 1.0;
double t0;

/* some definitions from hw2.h
 */
#ifdef Q3
int q3_usleep(int usecs);
#define usleep(n) q3_usleep(n)
#endif


/* handler - ^C will break out of usleep() below; set flag to stop
 * loop 
 */
static int done;
static void handler(int sig)
{
    signal(SIGINT, SIG_DFL);
    done = 1;
}

#ifdef Q2
static void init_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t0 = tv.tv_sec + tv.tv_usec/1.0e6;
}

/* timestamp - time since start - NOT adjusted for speedup
 */
double timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t1 = tv.tv_sec + tv.tv_usec/1.0e6;
    return (t1-t0);
}

/* wait until simulation end or ^C
 */
void wait_until_done(void)
{
    while (!done && (!end_time || timestamp() < end_time / speedup))
        usleep(100000);
}

/* sleep_exp(T) - sleep for exp. dist. time with mean T secs
 *                unlocks mutex while sleeping if provided.
 */
#include <pthread.h>

void sleep_exp(double T, void *m)
{
    double t = -1 * T * log(drand48()); /* sleep time */
    if (t > T*10)
        t = T*10;
    if (t < 0.002 * speedup)
        t = 0.002 * speedup;
    if (m != NULL)
        pthread_mutex_unlock(m);
    usleep(1000000 * t / speedup);
    if (m != NULL)
        pthread_mutex_lock(m);
}

#endif

#ifdef Q3
#include "hw2.h"

void sleep_exp(double T, void *m)
{
    double t = -1 * T * log(drand48()); /* sleep time */

    if (m != NULL)
        pth_mutex_release(m);
    usleep(1000000 * t);
    if (m != NULL)
        pth_mutex_acquire(m, FALSE, NULL);
}

/* Thread book-keeping. We need to keep track of how many runnable
 * threads there are at any moment; if the last thread goes to sleep,
 * then we need to step the simulation time forward to the first
 * waiting event.
 */
static int nthreads = 1;        /* starts with main thread running */
static double now = 0.0;

double timestamp(void)
{
    return now;
}

/* Wait structures. These form a doubly-linked list, sorted by
 * increasing expiration time. The thread is waiting to be signalled
 * with SIGUSR1.
 */
struct q3_wait {
    pth_t        th;            /* sleeping thread */
    double       t;             /* expiration time */
    struct q3_wait *prev, *next;
    pth_cond_t   cond;
    int          done;
};
static struct q3_wait waitq = {.next = &waitq, .prev = &waitq};
static pth_mutex_t wait_mutex = PTH_MUTEX_INIT;

/* List manipulation functions. 
 */
static void insert_before(struct q3_wait *next, struct q3_wait *this)
{
    this->next = next;
    this->prev = next->prev;
    next->prev->next = this;
    next->prev = this;
}
static struct q3_wait *remove_after(struct q3_wait *p)
{
    if (p->prev == p)
        return NULL;
    struct q3_wait *w = p->next;
    p->next = w->next;
    p->next->prev = w->prev;
    return w;
}

/* Wake the first thread from the timer queue and jump simulation time
 * forward. 
 */
static void q3_wake_next(struct q3_wait *self)
{
    struct q3_wait *next = remove_after(&waitq);
    now = next->t;
    next->done = 1;
    nthreads++;
    pth_cond_notify(&next->cond, FALSE);
}

/* Go to sleep on the timer queue for N simulated microseconds
 */
int q3_usleep(int usecs)
{
    double t = usecs / 1000000.0;
    struct q3_wait w = {.th = pth_self(), .t = now+t, .done = 0,
			.cond = PTH_COND_INIT};
    struct q3_wait *p;

    /* Find our position in the sorted list. This is where
     * doubly-linked lists are nice - you don't have to worry about
     * corner conditions at the head and tail.
     */
    for (p = waitq.next; p != &waitq && p->t < w.t; )
        p = p->next;

    /* If we're the last thread, then either we'll be the next one and
     * have to return, or we need to release the first thread before
     * we go to sleep.
     */
    if (nthreads == 1 && p->prev == &waitq) {
	now = w.t;
	return 0;
    }

    /* Insert ourselves in the timer queue and go to sleep. All timer
     * queue and sleep/wakeup operations are protected by wait_mutex.
     */
    pth_mutex_acquire(&wait_mutex, FALSE, NULL);

    insert_before(p, &w);
    nthreads--;
    if (nthreads == 0)
	q3_wake_next(&w);

    while (!w.done)
	pth_cond_await(&w.cond, &wait_mutex, NULL);

    pth_mutex_release(&wait_mutex);

    return 0;
}

/* Wrapper functions for mutexes. For a more general simulator we
 * would probably want to track the number of threads waiting on a
 * mutex, like we do for condvars. For monitor-structured code this
 * isn't an issue.
 */
int q3_mutex_lock(pth_mutex_t *mutex)
{
    return pth_mutex_acquire(mutex, FALSE, NULL);
}

int q3_mutex_unlock(pth_mutex_t *mutex)
{
    return pth_mutex_release(mutex);
}

/* Wrapper functions for condition variables. Note that we keep track
 * of the number of threads waiting so that we know when there are no
 * more runnable events and we have to take the next event from the
 * timer queue.
 */
int q3_cond_init(q3_cond_t *c)
{
    c->waiting = 0;
    return pth_cond_init(&c->cond);
}

/* Pth conditions have a disturbing tendency to return when they're
 * not supposed to, so we need to make sure we keep sleeping until
 * we've actually been notified.
 */
struct cwait_q {
    struct cwait_th *head;
    struct cwait_th *tail;
};

struct cwait_th {
    pth_t           th;
    struct cwait_th *next;
    int             done;
};

/* simulated pthread_cond_wait
 */
int q3_cond_wait(q3_cond_t *c, pth_mutex_t *mutex)
{
    if (c->private == NULL)
	c->private = calloc(sizeof(struct cwait_q), 1);

    /* Queue self on the condition
     */
    struct cwait_q *q = c->private;
    struct cwait_th self = {.th = pth_self(), .next = NULL, .done = 0};
    if (q->head) {
	q->tail->next = &self;
	q->tail = &self;
    }
    else
	q->head = q->tail = &self;

    /* if we're the last thread runnable, wake the first thread in the
     * timer queue.
     */
    pth_mutex_acquire(&wait_mutex, FALSE, NULL);
    if (--nthreads == 0)
        q3_wake_next(NULL);
    pth_mutex_release(&wait_mutex);

    /* And now wait until we're signalled.
     * (is c->waiting redundant now???)
     */
    c->waiting++;
    while (!self.done)
	pth_cond_await(&c->cond, mutex, NULL);
    
    /* Note that c->waiting-- and nthreads++ happen in the call to
     * q3_signal / q3_broadcast that wakes us up. 
     */

    return 0;			/* success */
}

/* signal - mark the first waiting thread as done and wake it.
 */
int q3_cond_signal(q3_cond_t *c)
{
    if (c->waiting > 0) {
        c->waiting--;
        nthreads++;

	struct cwait_q *q = c->private;
	q->head->done = 1;
	q->head = q->head->next;
    }

    return pth_cond_notify(&c->cond, FALSE); /* broadcast=FALSE */
}

/* broadcast - mark all waiting threads as done and wake them.
 */
int q3_cond_broadcast(q3_cond_t *c)
{
    if (c->waiting > 0) {
        nthreads += c->waiting;
        c->waiting = 0;

	struct cwait_q *q = c->private;
	while (q->head != NULL) {
	    q->head->done = 1;
	    q->head = q->head->next;
	}
    }

    return pth_cond_notify(&c->cond, TRUE); /* broadcast=TRUE */
}

/* Thread creation. We use a thunk to track thread completion so that
 * nthreads stays in sync. (we hope)
 */
struct q3_thunk {
    void *arg;
    void *(*f)(void*);
};

static void* run_thunk(void *ctx)
{
    struct q3_thunk *t = ctx;
    void *arg = t->arg;
    void *(*f)(void*) = t->f;
    free(t);
    void *val = f(arg);
    nthreads--;
    return val;
}

int q3_create(pth_t *thread, pth_attr_t attr, 
               void *(*start_routine)(void*), void *arg)
{
    struct q3_thunk *t = malloc(sizeof(*t));
    nthreads++;
    t->arg = arg;
    t->f = start_routine;
    *thread = pth_spawn(attr, run_thunk, t);
    return 0;                   /* always succeeds */
}

void wait_until_done(void)
{
    for (;;) {
        if (end_time > 0 && now > end_time)
            break;
        if (done)
            break;
        q3_usleep(1000000);
    }
}

struct stat_count {
    double t;
    int count;
    double sum;
};

/* Create a new counter object. Free it using free()
 */
void *stat_counter(void)
{
    struct stat_count *sc = calloc(sizeof(*sc), 1);
    return sc;
}

static void stat_count_change(void *ctr, int val)
{
    struct stat_count *sc = ctr;
    sc->sum += sc->count * (now - sc->t);
    sc->count += val;
    sc->t = now;
}

/* increment a counter
 */
void stat_count_incr(void *ctr)
{
    stat_count_change(ctr, 1);
}

/* decrement a counter
 */
void stat_count_decr(void *ctr)
{
    stat_count_change(ctr, -1);
}

/* mean value of a counter from time 0 until now.
 */
double stat_count_mean(void *ctr)
{
    struct stat_count *sc = ctr;
    double sum = sc->sum + sc->count * (now - sc->t);
    return sum / now;
}

#define TMR_MAX 20
struct stat_timer {
    struct {
        pth_t thread;
        double t;
    } waiters[TMR_MAX];
    int count;
    double sum;
};

/* Create a new timer object. Free it using free()
 */
void *stat_timer(void)
{
    struct stat_timer *st = calloc(sizeof(*st), 1);
    return st;
}

/* Start a timer for the current thread.
 */
void stat_timer_start(void *tmr)
{
    int i;
    struct stat_timer *st = tmr;
    for (i = 0; i < TMR_MAX; i++) 
        if (st->waiters[i].thread == NULL)
            break;
    assert(i < TMR_MAX);
    st->waiters[i].thread = pth_self();
    st->waiters[i].t = now;
}

/* Stop the timer for the current thread.
 */
void stat_timer_stop(void *tmr)
{
    int i;
    struct stat_timer *st = tmr;
    for (i = 0; i < TMR_MAX; i++) 
        if (st->waiters[i].thread == pth_self())
            break;
    assert(i < TMR_MAX);
    st->count++;
    st->sum += (now - st->waiters[i].t);
    st->waiters[i].thread = NULL;
}

/* Mean value - i.e. the average time between when a thread calls
 * timer_start() and when it calls timer_stop()
 */
double stat_timer_mean(void *tmr)
{
    struct stat_timer *st = tmr;
    return st->sum / st->count;
}

#endif

int main(int argc, char **argv)
{
    int i, seed = 0;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
#ifdef Q2
        if (!strcmp(argv[i], "-speedup"))
            speedup = atof(argv[++i]);
#endif
        if (!strcmp(argv[i], "-seed"))
            seed = atoi(argv[++i]);
        if (!strcmp(argv[i], "-rand"))
            seed = time(NULL);
    }
    if (i < argc)
        end_time = atoi(argv[i]);

#ifdef Q2
    printf("speedup %.2f\n", speedup);
#endif
    if (end_time > 0)
        printf("duration %d\n", end_time);

    if (seed != 0)
        srand48(seed);


#ifdef Q3
    pth_init();
    signal(SIGINT, handler);
    q3();
#endif
    
#ifdef Q2
    signal(SIGINT, handler);
    init_time();
    q2();
    exit(0);
#endif

    return 0;
}


