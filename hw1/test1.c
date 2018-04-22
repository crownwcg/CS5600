#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "qthread.h"

/*
mutex lock/unlock
calling lock when the mutex is: unlocked, locked with no threads waiting, locked with one thread waiting, locked with two threads waiting
again, you'll probably have to use qthread_usleep to set up these scenarios
make sure that you call qthread_usleep and/or qthread_yield while holding the mutex, and increment a global variable or something inside the mutex so you can check to make sure no one else has entered the mutex.

cond wait/signal/broadcast
calling wait() when there are 0, 1, and 2 threads already waiting
calling signal() when there are 0, 1, 2, and 3 threads waiting
calling broadcast() with 0,1,2,3 threads waiting
Again you'll probably need to use qthread_usleep to coordinate things. For this and the mutex test, it's likely that if the test fails your code is just going to hang. That's OK, just fix it. (I'm the only one who has to come up with code that can run all the tests, even if some of them fail)

qthread_usleep, qthread_read 
I'll trust that your _write and _accept code works if you got _read to work
usleep - test with 1, 2, and 3 threads sleeping simultaneously, with (a) equal timeouts and (b) different timeouts, that they wait "about" the right amount of time. (e.g. use get_usecs to time how long the call to qthread_usleep blocks before returning)
for testing read you can create a pipe (see 'man 2 pipe') - it generates a read and write file descriptor; you can have one thread call read() on the read descriptor, wait a while, and then have another thread call write() on the other descriptor
run usleep tests under two cases: (a) no threads waiting for I/O, and (b) one thread blocked in qthread_read
test read with 1, 2, and 3 threads calling read (on separate pipes) at the same time. Do it (a) without, and (b) with one thread blocked in qthread_usleep
*/

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
        qthread_yield();
        printf("%s\n", (char*)arg);
    }
    return arg;
}

void test1(void)
{
    qthread_t t = qthread_create(run_test1, "1");
    qthread_run();
    void *val = qthread_join(t);
    assert(!strcmp(val, "1"));

    qthread_t t2[2] = {qthread_create(run_test1, "1"),
                       qthread_create(run_test1, "2")};
    qthread_run();
    val = qthread_join(t2[0]);
    assert(!strcmp(val, "1"));
    val = qthread_join(t2[1]);
    assert(!strcmp(val, "2"));

    qthread_t t3[3] = {qthread_create(run_test1, "1"),
                       qthread_create(run_test1, "2"),
                       qthread_create(run_test1, "3")};
    qthread_run();
    val = qthread_join(t3[0]);
    assert(!strcmp(val, "1"));
    val = qthread_join(t3[1]);
    assert(!strcmp(val, "2"));
    val = qthread_join(t3[2]);
    assert(!strcmp(val, "3"));
}    
    
        
int main(int argc, char** argv)
{
    test1();
}
