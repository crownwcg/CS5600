#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "qthread.h"

#include <sys/time.h>
#include <unistd.h>

static double get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec/1.0e6;
}

/*
mutex lock/unlock
calling lock when the mutex is: unlocked, locked with no threads waiting, locked with one thread waiting, locked with two threads waiting
again, you'll probably have to use qthread_usleep to set up these scenarios
make sure that you call qthread_usleep and/or qthread_yield while holding the mutex, and increment a global variable or something inside the mutex so you can check to make sure no one else has entered the mutex.
*/
struct test4 {
    qthread_mutex_t m;
    int count;
};

void *run_test4_1(void *arg)
{
    struct test4 *t4 = arg;
    qthread_mutex_lock(&t4->m);
    int i = t4->count;
    qthread_usleep(100000);
    qthread_yield();
    t4->count = i+1;
    qthread_mutex_unlock(&t4->m);
}
    
int test4_ran = 0;
void *test4_tmp(void *arg)
{
    int i, j;
    qthread_t t[4];
    struct test4 t4;

    test4_ran = 1;
    
    memset(&t4, 0, sizeof(t4));

    for (i = 1; i < 5; i++) {
        t4.count = 0;
        qthread_mutex_init(&t4.m);
        for (j = 0; j < i; j++) 
            t[j] = qthread_create(run_test4_1, &t4);

        for (j = 0; j < i; j++)
            qthread_join(t[j]);
        assert(t4.count == i);
    }
}

void test4(void)
{
    qthread_create(test4_tmp, NULL);
    qthread_run();
    assert(test4_ran == 1);
    printf("TEST 4: passed\n");
}

/*
cond wait/signal/broadcast
calling wait() when there are 0, 1, and 2 threads already waiting
calling signal() when there are 0, 1, 2, and 3 threads waiting
calling broadcast() with 0,1,2,3 threads waiting
Again you'll probably need to use qthread_usleep to coordinate things. For this and the mutex test, it's likely that if the test fails your code is just going to hang. That's OK, just fix it. (I'm the only one who has to come up with code that can run all the tests, even if some of them fail)
*/

struct test5 {
    int flag;
    qthread_mutex_t m;
    qthread_cond_t c;
};

void *run_test5_1(void *arg)
{
    struct test5 *t5 = arg;
    qthread_mutex_lock(&t5->m);
    assert(t5->flag == 0);
    t5->flag = 1;
    qthread_usleep(100000);
    t5->flag = 0;
    qthread_cond_wait(&t5->c, &t5->m);
    assert(t5->flag == 0);
    t5->flag = 1;
    qthread_usleep(100000);
    t5->flag = 0;
    qthread_mutex_unlock(&t5->m);
}

void *run_test5_2(void *arg)
{
    struct test5 *t5 = arg;
    qthread_usleep(100000);
    qthread_mutex_lock(&t5->m);
    assert(t5->flag == 0);
    qthread_cond_broadcast(&t5->c);
    qthread_usleep(100000);
    assert(t5->flag == 0);
    qthread_mutex_unlock(&t5->m);
    return NULL;
}

int test5_ran = 0;
void *test5_tmp(void *arg)
{
    int i, j, k;
    qthread_t t[6];
    struct test5 t5;

    test5_ran = 1;

    memset(&t5, 0, sizeof(t5));
    
    for (i = 1; i < 5; i++) {
        qthread_mutex_init(&t5.m);
        qthread_cond_init(&t5.c);
        
        t5.flag = 0;
        for (j = 0; j < i; j++) 
            t[j] = qthread_create(run_test5_1, &t5);
        t[j++] = qthread_create(run_test5_2, &t5);

        for (k = 0; k < j; k++)
            qthread_join(t[k]);
        assert(t5.flag == 0);
    }
}

void test5(void)
{
    qthread_create(test5_tmp, NULL);
    qthread_run();
    assert(test5_ran == 1);
    printf("TEST 5: passed\n");
}

/*
qthread_usleep, qthread_read 
I'll trust that your _write and _accept code works if you got _read to work
usleep - test with 1, 2, and 3 threads sleeping simultaneously, with (a) equal timeouts and (b) different timeouts, that they wait "about" the right amount of time. (e.g. use get_usecs to time how long the call to qthread_usleep blocks before returning)
for testing read you can create a pipe (see 'man 2 pipe') - it generates a read and write file descriptor; you can have one thread call read() on the read descriptor, wait a while, and then have another thread call write() on the other descriptor
run usleep tests under two cases: (a) no threads waiting for I/O, and (b) one thread blocked in qthread_read
test read with 1, 2, and 3 threads calling read (on separate pipes) at the same time. Do it (a) without, and (b) with one thread blocked in qthread_usleep
*/

void *run_test3_1(void *arg)
{
    int *fd = arg;
    char c;
    int val = qthread_read(fd[0], &c, 1);
    assert(val == 1 && c == 17);
    return NULL;
}

void *run_test3_2(void *arg)
{
    int *fd = arg;
    char c = 17;
    qthread_usleep(100000);
    qthread_write(fd[1], &c, 1);
    return NULL;
}

void *run_test3_3(void *arg)
{
    int *fd = arg;
    int i, c = 17;
    for (i = 0; i < 3; i++) {
        qthread_usleep(100000);
        qthread_write(fd[2*i+1], &c, 1);
    }
    return NULL;
}

void *run_test3_4(void *arg)
{
    qthread_usleep(100000);
    return NULL;
}

int test3_ran = 0;
void *test3_tmp(void *arg)
{
    int fd[6];
    test3_ran = 1;
    pipe(fd);
    qthread_t t1 = qthread_create(run_test3_1, fd);
    qthread_t t2 = qthread_create(run_test3_2, fd);
    qthread_join(t2);
    qthread_join(t1);

    int i;
    pipe(&fd[2]); pipe(&fd[4]);
    qthread_t t3[3] = {qthread_create(run_test3_1, fd),
                       qthread_create(run_test3_1, &fd[2]),
                       qthread_create(run_test3_1, &fd[4])};
    qthread_t t4 = qthread_create(run_test3_3, fd);

    for (i = 0; i < 3; i++)
        qthread_join(t3[i]);
    qthread_join(t4);

    qthread_t t5[4] = {qthread_create(run_test3_4, NULL),
                       qthread_create(run_test3_1, fd),
                       qthread_create(run_test3_1, &fd[2]),
                       qthread_create(run_test3_1, &fd[4])};
    t4 = qthread_create(run_test3_3, fd);

    for (i = 0; i < 4; i++)
        qthread_join(t5[i]);
    qthread_join(t4);

    for (i = 0; i < 6; i++)
        close(fd[i]);
}

void test3(void)
{
    qthread_create(test3_tmp, NULL);
    qthread_run();
    assert(test3_ran == 1);
    printf("TEST 3: passed\n");
}

void* run_test2(void* arg)
{
    int i;
    for (i = 0; i < 3; i++) {
        qthread_usleep(100000);
    }
    return arg;
}

int test2_ran = 0;
void *test2_tmp(void*arg)
{
    test2_ran = 1;
    double t1 = get_time();
    qthread_t t3[3] = {qthread_create(run_test2, "1"),
                       qthread_create(run_test2, "2"),
                       qthread_create(run_test2, "3")};
    qthread_join(t3[0]);
    qthread_join(t3[1]);
    qthread_join(t3[2]);
    double t2 = get_time();
    double t = t2-t1;

    assert(20*0.010 < t && t < 40*0.01);
}

void test2(void)
{
    qthread_create(test2_tmp, NULL);
    qthread_run();
    assert(test2_ran == 1);
    printf("TEST 2: passed\n");
}

    
/*
  create/yield/join/exit, make sure you test the following cases:
    1 thread, yield several times, then exit
    2 threads, yield back and forth a few times, both exit
    3 or more threads, same
    call join before the child thread exits
    call join after the child thread exits (you may want to use qthread_usleep for this)
*/
void* run_test1(void* arg)
{
    int i;
    for (i = 0; i < 3; i++) {
        //qthread_usleep(300000);
        qthread_yield();
        //printf("%s\n", (char*)arg);
    }
    return arg;
}

void *join_test1(void *arg)
{
    qthread_t *t = arg;
    int i;
    for (i = 0; i < 3; i++) {
        char *val = qthread_join(t[i]);
        assert(val[0] == '1' + i);
    }
    return 0;
}

int test1_ran = 0;
void *test1_tmp(void *arg)
{
    test1_ran = 1;
    qthread_t t = qthread_create(run_test1, "1");
    void *val = qthread_join(t);
    assert(!strcmp(val, "1"));

    qthread_t t2[2] = {qthread_create(run_test1, "1"),
                       qthread_create(run_test1, "2")};

    val = qthread_join(t2[0]);
    assert(!strcmp(val, "1"));
    val = qthread_join(t2[1]);
    assert(!strcmp(val, "2"));

    qthread_t t3[3] = {qthread_create(run_test1, "1"),
                       qthread_create(run_test1, "2"),
                       qthread_create(run_test1, "3")};

    val = qthread_join(t3[0]);
    assert(!strcmp(val, "1"));
    val = qthread_join(t3[1]);
    assert(!strcmp(val, "2"));
    val = qthread_join(t3[2]);
    assert(!strcmp(val, "3"));

    qthread_t t4[3] = {qthread_create(run_test1, "1"),
                       qthread_create(run_test1, "2"),
                       qthread_create(run_test1, "3")};
    qthread_t t5 = qthread_create(join_test1, t4);
    val = qthread_join(t5);
    assert(val == NULL);
}

void test1(void)
{
    qthread_create(test1_tmp, NULL);
    qthread_run();
    assert(test1_ran == 1);
    printf("TEST 1: passed\n");
}
    
int main(int argc, char** argv)
{
    if (argc == 1){
        printf("Give a set of tests numbers to run between 1-5, e.g '1' for test 1, or '134' for test 1, 3 and 4\n");
        return 0;
    }

    while (argv[1] && argv[1][0]) {
        char c = *(argv[1]++);
    switch (c){
    case '1':
        test1(); break;
    case '2':
        test2(); break;
    case '3':
        test3(); break;
    case '4':
        test4(); break;
    case '5':
        test5(); break;
        default:
            printf("No such test: %c\n", c);
            break;
        }
    }
}
