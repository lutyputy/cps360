
#include <assert.h>
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>
#include "ut_threads.h"

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
} uthread_t;


/*
* Define the thread table
* Thread ID's are indexes into this table
*/
uthread_t thread[MAX_THREADS]; // the thread table

int curThread; // the index of the currently executing thread

// Do needed initialization work, including setting up stack pointers for all of the threads
int ut_init(char *stackbuf) {
  int i;
  // setup stack pointers
  for (i = 0 ; i < MAX_THREADS; ++i ) {
    thread[i].context.uc_stack.ss_sp = stackbuf + i * STACK_SIZE;
    thread[i].context.uc_stack.ss_size = STACK_SIZE;
  }
  
  // initialize main thread
  thread[0].status = THREAD_ALIVE;
  curThread = 0;
  return 0;
}

// Creates a thread with entry point set to <entry>
// <arg> is the argument that will be passed to <entry> in the new thread
// Returns threadID on success, or -1 on failure (thread table full)
int ut_create(void (* entry)(int), int arg)
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

    // and initialize its context TODO
    getcontext(&thread[i].context);
    makecontext(&thread[i].context, (void *)&entry, 1, arg);

    // Return the thread Id
    return i;
}

// scheduler - picks a thread to run (possibly this one, if no other threads are available)
void ut_yield()
{
    int newThread = -1;
    int i = (curThread + 1) % 10;

  // find a thread that can run, using round robin scheduling; pick this one if no other thread can run
    while(newThread == -1 && i != curThread)
    {
        //printf("%d\n", thread[i].status);
        i = ++i % MAX_THREADS;
        if(thread[i].status == 1)
        {
            newThread = i;
        }
    }
    // if another thread can run, switch to it && otherwise, return to continue running this thread
    if(newThread != -1)
    {
       
         printf("%d %d\n", curThread, newThread);
        swapcontext(&thread[curThread].context, &thread[newThread].context);
         printf("%d %d\n", curThread, newThread);
    }
    else   // if no threads are ALIVE, exit the program
    {
  
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
    // record <result> in current thread's exitValue field
    thread[curThread].exitValue = result;

    // set current thread's status to THREAD_ZOMBIE
    thread[curThread].status = 2;

    // pick another thread to run
    ut_yield();
}