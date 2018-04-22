/*
 * file:        pthread.h
 * description: POSIX threads in simulation time for CS 5600 HW2
 *
 * Peter Desnoyers, Northeastern CCIS, 2011
 * $Id: hw2.h 525 2012-01-31 18:58:08Z pjd $
 */
#ifndef __HW2_H__
#define __HW2_H__

/* For Q2 we use the normal Pthreads library; for Q3 the user-space
 * GNU Pth library.  
 */
#ifdef Q2
#include <pthread.h>
#endif

#ifdef Q3
#include <pth.h>
#endif

/* general functions from misc.c
 */
extern void sleep_exp(double T, void *m);
extern double timestamp(void);
extern void wait_until_done(void);

/* This defines is a Pthreads-compatible simulation system running on
 * top of Pth.
 */
#ifdef Q3

typedef struct {
    int        waiting;		/* need to track #threads waiting */
    pth_cond_t cond;            /* see misc.c for details */
    void      *private;
} q3_cond_t;

/* functions defined in misc.c
 */

extern int q3_mutex_lock(pth_mutex_t *mutex);
extern int q3_mutex_unlock(pth_mutex_t *mutex);
extern int q3_cond_init(q3_cond_t *c);
extern int q3_cond_wait(q3_cond_t *c, pth_mutex_t *mutex);
extern int q3_cond_signal(q3_cond_t *c);
extern int q3_cond_broadcast(q3_cond_t *c);
extern int q3_create(pth_t *thread, pth_attr_t attr, 
                      void *(*start_routine)(void*), void *arg);
extern int q3_usleep(int);

/* define compatible replacements for standard pthread functions 
 */
#define Q3_COND_INIT {.waiting = 0, .cond = PTH_COND_INIT}
#define pthread_t pth_t
#define pthread_create(th, attr, f, arg) q3_create(th, attr, f, arg)
#define pthread_mutex_t pth_mutex_t
#define PTHREAD_MUTEX_INITIALIZER PTH_MUTEX_INIT
#define pthread_mutex_init(mutex, attr) pth_mutex_init(mutex)
#define pthread_mutex_lock(m) q3_mutex_lock(m)
#define pthread_mutex_unlock(m) q3_mutex_unlock(m)
#define pthread_cond_t q3_cond_t
#define PTHREAD_COND_INITIALIZER Q3_COND_INIT
#define pthread_cond_init(cond, attr) q3_cond_init(cond)
#define pthread_cond_wait(cond, mutex) q3_cond_wait(cond, mutex)
#define pthread_cond_signal(cond) q3_cond_signal(cond)
#define pthread_cond_broadcast(cond) q3_cond_broadcast(cond)
#define pthread_join(thr, vptr) pth_join(thr, vptr)
#define usleep(n) q3_usleep(n)

#endif  /* Q3 */

#ifdef Q3
/* statistics functions - see PDF file for description. 
 */
extern void *stat_counter(void);
extern void stat_count_incr(void *ctr);
extern void stat_count_decr(void *ctr);
extern double stat_count_mean(void *ctr);
extern void *stat_timer(void);
extern void stat_timer_start(void *tmr);
extern void stat_timer_stop(void *tmr);
extern double stat_timer_mean(void *tmr);
#endif  /* Q3 */

#ifdef Q2
/* null functions so we can leave Q3 statistics code in place for
 * question 2.
 */
static inline void *stat_counter(void) {return NULL;}
static inline void stat_count_incr(void *ctr){}
static inline void stat_count_decr(void *ctr){}
static inline double stat_count_mean(void *ctr){return 0.0;}
static inline void *stat_timer(void){return NULL;}
static inline void stat_timer_start(void *tmr){}
static inline void stat_timer_stop(void *tmr){}
static inline double stat_timer_mean(void *tmr){return 0.0;}
#endif  /* Q2 */

#endif  /* __HW2_H__ */
