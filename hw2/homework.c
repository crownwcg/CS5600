/* 
 * file:        homework.c
 * description: Skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern CCIS, 2011
 * $Id: homework.c 530 2012-01-31 19:55:02Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hw2.h"

void print_barber_sleep(){
    printf("DEBUG: %f barber goes to sleep\n", timestamp());
}

void print_barber_wakes_up(){
    printf("DEBUG: %f barber wakes up\n", timestamp());
}

void print_customer_enters_shop(int customer){
    printf("DEBUG: %f customer %d enters shop\n", timestamp(), customer);
}

void print_customer_starts_haircut(int customer){
    printf("DEBUG: %f customer %d starts haircut\n", timestamp(), customer);
}

void print_customer_leaves_shop(int customer){
    printf("DEBUG: %f customer %d leaves shop\n", timestamp(), customer);
}

/********** YOUR CODE STARTS HERE ******************/
#define TIME_OF_HAIRCUT 1.2
#define TIME_OF_CUSTOMER_CIRCLE 10
#define NUM_OF_WAIT_CHAIRS 4
#define NUM_OF_BARBER_CHAIR 1
#define NUM_OF_CUSTOMERS 10
#define WAIT_LINE_INITIALIZER { .head = NULL, .tail = NULL, .size = 0 }

/* queue node 
 */
typedef struct node {
    int index;
    struct node *next;
} q_node;

/* customers wating queue 
 */
typedef struct queue {
    q_node *head, *tail; 
    size_t size;
} wait_line;

pthread_mutex_t m       = PTHREAD_MUTEX_INITIALIZER;// mutex 
pthread_cond_t barber_c = PTHREAD_COND_INITIALIZER; // barber 
pthread_cond_t wait_c   = PTHREAD_COND_INITIALIZER; // customers who are waiting in line
pthread_cond_t done_c   = PTHREAD_COND_INITIALIZER; // customers who are cutting hair
wait_line      line     = WAIT_LINE_INITIALIZER;    // wait line of customers

bool is_sleep    = true; // whether the barber is sleep
size_t total     = 0;    // total number of customers comes
size_t turn_away = 0;    // fraction of customer visits result in turning away
void *counter_in_shop;   // average number of customers in the shop
void *timer_in_shop;     // average time spent in the shop
void *counter_in_chair;  // fraction of time someone is sitting in the barber's chair 

/* get the first customer of the queue
 */
static int q_poll(wait_line *q) {
    if (q == NULL || q->head == NULL) {
        return -1;
    }  
    // get first node's index
    int index = q->head->index;
    // delete the node
    free(q->head);
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    q->size--;
    return index;
}

/* find the first customer index of the queue
 */
static int q_peek(wait_line *q) {
    if (q == NULL || q->head == NULL) {
        return -1;
    }  
    return q->head->index;
}

/* put a customer in the waiting queue
 */
static void q_offer(wait_line *q, int index) {
    if (q == NULL) {
        return;
    }
    // initiate queue node
    q_node *node = malloc(sizeof(*node));
    node->index = index;
    node->next = NULL;
    // add node
    if (q->head == NULL) {
        q->head = q->tail = node; 
    } else {
        q->tail->next = node;
        q->tail = q->tail->next;
    }
    q->size++;
}

/* check whether the waiting queue is full
 */
static bool q_full(wait_line *q) {
    return q == NULL ? false : 
        q->size == NUM_OF_WAIT_CHAIRS + NUM_OF_BARBER_CHAIR;
}

/* check whether the waiting queue is empty
 */
static bool q_empty(wait_line *q) {
    return q == NULL ? true : q->size == 0;
}

/* the barber method
 */
void barber(void) {
    pthread_mutex_lock(&m);
    while (true) {
        // wait until woke up by customer
        while (q_empty(&line)) {
            print_barber_sleep();
            is_sleep = true;
            pthread_cond_wait(&barber_c, &m);
            is_sleep = false;
            print_barber_wakes_up();
        }

        // cut customer's hair
        int index = q_peek(&line);
        print_customer_starts_haircut(index);
        stat_count_incr(counter_in_chair);
        sleep_exp(TIME_OF_HAIRCUT, &m);

        // cutting is finished: 
        // customer leaves and signal next waiting customer
        pthread_cond_signal(&done_c);
        pthread_cond_signal(&wait_c);
        q_poll(&line);
        stat_count_decr(counter_in_shop);
        stat_count_decr(counter_in_chair);
        print_customer_leaves_shop(index);
    }
    pthread_mutex_unlock(&m);
}

/* the customer method
 */
void customer(int customer_num) {
    pthread_mutex_lock(&m);
    total++;
    if (!q_full(&line)) {
        stat_count_incr(counter_in_shop);
        stat_timer_start(timer_in_shop);

        // customer enters and wake up the barber
        if (is_sleep) {
            pthread_cond_signal(&barber_c);
        }
        q_offer(&line, customer_num);
        print_customer_enters_shop(customer_num);

        // wait in line until other customers finished
        while (customer_num != q_peek(&line)) {
            pthread_cond_wait(&wait_c, &m);
        }

        // wait for hair cut
        pthread_cond_wait(&done_c, &m);
        stat_timer_stop(timer_in_shop);
    } else {
        // shop is full, leave immediately
        turn_away++;
    }
    pthread_mutex_unlock(&m);
}

/* Threads which call these methods. Note that the pthread create
 * function allows you to pass a single void* pointer value to each
 * thread you create; we actually pass an integer (the customer number)
 * as that argument instead, using a "cast" to pretend it's a pointer.
 */

/* the customer thread function - create 10 threads, each of which calls
 * this function with its customer number 0..9
 */
void *customer_thread(void *context) 
{
    int customer_num = (int)context; 
    while (true) {
        // simulate customer behavior
        sleep_exp(TIME_OF_CUSTOMER_CIRCLE, NULL);
        customer(customer_num);
    }
    return context;
}

/*  barber thread
 */
void *barber_thread(void *context)
{
    barber(); /* never returns */
    return 0;
}

void q2(void)
{
    pthread_t barber_t;
    pthread_t customers_t[NUM_OF_CUSTOMERS];

    pthread_create(&barber_t, NULL, barber_thread, NULL);
    for (long i = 0; i < NUM_OF_CUSTOMERS; i++) {
        pthread_create(&customers_t[i], NULL, customer_thread, (void *) i);
    }

    wait_until_done();
}

/* For question 3 you need to measure the following statistics:
 *
 * 1. fraction of  customer visits result in turning away due to a full shop 
 *    (calculate this one yourself - count total customers, those turned away)
 * 2. average time spent in the shop (including haircut) by a customer 
 *     *** who does not find a full shop ***. (timer)
 * 3. average number of customers in the shop (counter)
 * 4. fraction of time someone is sitting in the barber's chair (counter)
 *
 * The stat_* functions (counter, timer) are described in the PDF. 
 */

void q3(void)
{
    // trace stat
    counter_in_shop  = stat_counter();
    timer_in_shop    = stat_timer();
    counter_in_chair = stat_counter();

    // create threads
    pthread_t barber_t;
    pthread_t customers_t[NUM_OF_CUSTOMERS];

    pthread_create(&barber_t, NULL, barber_thread, NULL);
    for (long i = 0; i < NUM_OF_CUSTOMERS; i++) {
        pthread_create(&customers_t[i], NULL, customer_thread, (void *) i);
    }

    wait_until_done();

    printf("Fraction of  customer visits result in turning away: %.2f\n", 
        turn_away / (double) total);
    printf("Average time spent in the shop: %.2f\n", 
        stat_timer_mean(timer_in_shop));
    printf("Average number of customers in the shop: %.2f\n", 
        stat_count_mean(counter_in_shop));
    printf("Fraction of time someone is sitting in the barber's chair: %.2f\n", 
        stat_count_mean(counter_in_chair));

    free(counter_in_shop);
    free(timer_in_shop);
    free(counter_in_chair);
}

