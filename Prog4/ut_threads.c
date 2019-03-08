
#include <assert.h>
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "ut_threads.h"

#include <signal.h>
#include <unistd.h>

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

/*
* Define the thread table
* Thread ID's are indexes into this table
*/
uthread_t thread[MAX_THREADS]; // the thread table
uthread_t pq[MAX_THREADS][MAX_THREADS];
ucontext_t rtrn;

int curThread; // the index of the currently executing thread

// Do needed initialization work, including setting up stack pointers for all of the threads
int ut_init(char *stackbuf) {
  int i;
  // setup stack pointers
  for (i = 0 ; i < MAX_THREADS; ++i ) {
    thread[i].context.uc_stack.ss_sp = stackbuf + i * STACK_SIZE;
    thread[i].context.uc_stack.ss_size = STACK_SIZE;
  }

    char returnStack[STACK_SIZE * MAX_THREADS];
    rtrn.uc_stack.ss_sp = returnStack;
    rtrn.uc_stack.ss_size = STACK_SIZE;
    getcontext(&rtrn);
    makecontext(&rtrn, ut_finish, 1, -1);
  
  // initialize main thread
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
        if(thread[i].status != 0){ continue; }
        else { break; }
    }

    // Return -1 if all slots are in use
    if(i + 1 == MAX_THREADS){ return -1; }
    
    // Otherwise, set the status of the slot to THREAD_ALIVE
    thread[i].status = 1;

    for(int j = 0; j < MAX_THREADS; j++)
    {
        for(int k = 0; k < MAX_THREADS; k++)
        {
            printf("%d\n", k);
        }
    }

    // and initialize its context
    getcontext(&thread[i].context);
    thread[i].context.uc_link = &rtrn;
    thread[i].priority = (priority - 1) % MAX_THREADS;
    pq[thread[i].priority][i] = thread[i];
    makecontext(&thread[i].context, (void *)entry, 1, arg);

    // Return the thread Id
    return i;
}

// scheduler - picks a thread to run (possibly this one, if no other threads are available)
void ut_yield()
{
    int newThread = -1;
    int i = (curThread + 1) % MAX_THREADS;
    int oldThread = curThread;

  // find a thread that can run, using round robin scheduling; pick this one if no other thread can run
    while(newThread == -1 && i != curThread)
    {
        for(int j = MAX_THREADS - 1; j > -1; j--)
        {

        }
        //printf("%d\n", thread[i].status);
        if(thread[i].status == 1)
        {
            newThread = i;
        }
        i = ++i % MAX_THREADS;
    }
    // if another thread can run, switch to it && otherwise, return to continue running this thread
    if(newThread != -1)
    {
        curThread = newThread;
        swapcontext(&thread[oldThread].context, &thread[newThread].context);
        // printf("%d\n", ut_getid());
        // printf("START\n");
        // for(long int j = (long int)thread[curThread].context.uc_stack.ss_sp; j <  (long int)thread[curThread].context.uc_stack.ss_sp+ STACK_SIZE; j+=4)
        // {
        // printf("PRINTING STACK %ld\n", *(long int*)j);
        // }
        // printf("END\n");
    }
    else   // if no threads are ALIVE, exit the program
    {
        printf("IN YIELD\n");
        exit(1);
    }
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
    if(threadId >= MAX_THREADS || thread[threadId].status == 0) { return -1; }
    
    // busy-wait (calling ut_yield in a loop) while the status of thread <threadId> is THREAD_ALIVE
    while(thread[threadId].status == 1)
    {
        ut_yield();
    }
    //  printf("START\n");
    //     for(long int j = (long int)thread[1].context.uc_stack.ss_sp; j <  (long int)thread[1].context.uc_stack.ss_sp+ STACK_SIZE; j+=4)
    //     {
    //     printf("PRINTING STACK %ld\n", *(long int*)j);
    //     }
    //     printf("END\n");
    // If the thread status is THREAD_ZOMBIE,
    if(thread[threadId].status == 2)
    {
        //    Change its status to THREAD_UNUSED
        thread[threadId].status = 0;

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
    if(result == -1)
    {
         thread[curThread].exitValue =*(long int*)(thread[curThread].context.uc_stack.ss_sp+ STACK_SIZE - (13 * 4));
           thread[curThread].status = 2;
    }
    else
    {
        // record <result> in current thread's exitValue field
        thread[curThread].exitValue = result;

        // set current thread's status to THREAD_ZOMBIE
        thread[curThread].status = 2;
    }

    // pick another thread to run
    ut_yield();
}