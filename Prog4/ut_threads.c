
#include <assert.h>
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "ut_threads.h"

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

// The following are valid thread statuses:

// This thread is not in use
#define THREAD_UNUSED 0
// This thread has been started and has not finished
#define THREAD_ALIVE 1
// This thread has finished, but no one has called ut_join to obtain its status yet
#define THREAD_ZOMBIE 2

typedef struct {
    ucontext_t context;
    int status;   // one of THREAD_UNUSED, THREAD_ALIVE, THREAD_ZOMBIE
    int exitValue; // thread's return value
    int priority;  // thread's priority
} uthread_t;

typedef struct {
    int id[MAX_THREADS];    // array of thread ids
    int size;   // size of each queue
    int i;  // itteration counter
} pq_t;

/*
* Define the thread table
* Thread ID's are indexes into this table
*/
uthread_t thread[MAX_THREADS]; // the thread table
pq_t pq[MAX_THREADS];   // the priority queue

int curThread; // the index of the currently executing thread
struct itimerval time;  // the timer for preemptive scheduling

// Do needed initialization work, including setting up stack pointers for all of the threads
int ut_init(char *stackbuf) {
    int i;
    // setup stack pointers
    for (i = 0 ; i < MAX_THREADS; ++i ) {
        thread[i].context.uc_stack.ss_sp = stackbuf + i * STACK_SIZE;
        thread[i].context.uc_stack.ss_size = STACK_SIZE;
    }

    // set all slots in queue to -1
    for(int j = 0; j < MAX_THREADS; j++)
    {
        for(int k = 0; k < MAX_THREADS; k++)
        {
            pq[j].id[k] = -1;
        }
        pq[j].size = 0;
        pq[j].i = 0;
    }

    // set up timer
    time.it_interval.tv_sec = 0; 
    time.it_interval.tv_usec = 100000; 
    time.it_value.tv_sec = 0; 
    time.it_value.tv_usec = 100000; 

    signal(SIGVTALRM, ut_yield); 

    setitimer(ITIMER_VIRTUAL, &time, NULL);

    // initialize main thread
    getcontext(&thread[0].context);
    thread[0].priority = (1 - 1) % MAX_THREADS;     // add thread to queue
    pq[thread[0].priority].id[pq[thread[0].priority].size] = 0;
    pq[thread[0].priority].size++;
    thread[0].status = THREAD_ALIVE;
    curThread = 0;
    return 0;
}

// Creates a thread with entry point set to <entry>
// <arg> is the argument that will be passed to <entry> in the new thread
// Returns threadID on success, or -1 on failure (thread table full)
int ut_create(void (* entry)(int), int arg, int priority)
{
    int i;

    // Find a slot in thread table whose status is THREAD_UNUSED
    for (i = 0 ; i < MAX_THREADS; ++i ) {
        if(thread[i].status != THREAD_UNUSED){ continue; }
        else { break; }
    }

    // Return -1 if all slots are in use
    if(i == MAX_THREADS){ return -1; }
    
    // Otherwise, set the status of the slot to THREAD_ALIVE
    thread[i].status = THREAD_ALIVE;

    // and initialize its context
    getcontext(&thread[i].context);
    thread[i].priority = priority - 1 < 10 ? priority - 1 : 10;
    pq[thread[i].priority].id[pq[thread[i].priority].size] = i;
    pq[thread[i].priority].size++;
    makecontext(&thread[i].context, (void *)ut_return, 2, (void *)entry, arg);

    // Return the thread Id
    return i;
}

// scheduler - picks a thread to run (possibly this one, if no other threads are available)
void ut_yield()
{
    struct itimerval oldTime = {0}; 
    setitimer(ITIMER_VIRTUAL, &oldTime, &time); // pause timer because swapcontext is DANGEEROUS with SIGVTALRM
    int newThread = -1;
    int oldThread = curThread;
    int itt = 0;
    // find a thread that can run, using round robin scheduling; pick this one if no other thread can run
    while(newThread == -1)
    {
        for(int i = MAX_THREADS - 1; i > -1 && newThread == -1; i--)    // itterate over priority levels
        {
            int j = pq[i].i;    // get point to continue itterating
            for(int k = 0; k < pq[i].size && newThread == -1; k++)      // itterate over individual priorities
            {
                if(thread[pq[i].id[j]].status == THREAD_ALIVE)     // check if thread is THREAD_ALIVE
                {
                    newThread = pq[i].id[j];
                    pq[i].i = j + 1 == pq[i].size ? 0 : j + 1;
                }
                j = ++j % pq[i].size;
            }
            if(newThread == -1)
            {
                pq[i].i = pq[i].i + 1 == pq[i].size ? 0 : pq[i].i + 1;
            }
        }
        if(itt > 2)
        {
            newThread = 0;
        }
        itt++;
    }

    // if another thread can run, switch to it && otherwise, return to continue running this thread
    if(newThread != -1)
    {
        curThread = newThread;
        swapcontext(&thread[oldThread].context, &thread[newThread].context);

    }
    else   // if no threads are ALIVE, exit the program
    {
        exit(-1);
    }
    setitimer(ITIMER_VIRTUAL, &time, NULL);
}

// returns thread ID of current thread
int ut_getid()
{
    return curThread;  
}

// Wait for the thread identified by <threadId> to exit, returning its return value in status. 
// In case of success, this function returns 0, otherwise, -1.
int ut_join(int threadId, int *status) 
{
    // return -1 if threadId is illegal (out of range) or if its status is THREAD_UNUSED
    if(threadId >= MAX_THREADS || threadId < 0 || thread[threadId].status == THREAD_UNUSED) { return -1; }
    
    // busy-wait (calling ut_yield in a loop) while the status of thread <threadId> is THREAD_ALIVE
    while(thread[threadId].status == THREAD_ALIVE)
    {
        ut_yield();
    }
   
    // If the thread status is THREAD_ZOMBIE,
    if(thread[threadId].status == THREAD_ZOMBIE)
    {
        //    Change its status to THREAD_UNUSED
        thread[threadId].status = THREAD_UNUSED;

        //    Set *status to its exit code, and return 0
        *status = thread[threadId].exitValue;
        return 0;
    } 
    else // Otherwise, the thread was joined by someone else already; return -1
    {
        return -1;
    }
}

// Terminate execution of current thread
void ut_finish(int result)
{
    // record <result> in current thread's exitValue field
    thread[curThread].exitValue = result;

    // set current thread's status to THREAD_ZOMBIE
    thread[curThread].status = THREAD_ZOMBIE;

    // pick another thread to run
    ut_yield();
}

// Wrapper function that calls ut_finish even if user does not
void ut_return(int (* entry)(int), int arg)
{
    int rtrn = (*entry)(arg);
    ut_finish(rtrn);
}