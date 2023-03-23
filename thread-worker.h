// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#define QUANTUM 10000
#define PRIORITY_BOOST_TIME 50000
#define MLFQ_QUEUES_NUM 3
#define MLFQ_QUEUES_MOVE_ALL_TOP_TIME 10

typedef uint worker_t;

enum status{
	RUNNING,
	READY,
	BLOCKED_JOIN,
	BLOCKED_MUTEX,
	EXITED
}status;
enum mutex_status{
	INITIALIZED,
	LOCKED,
	UNLOCKED
}mutex_status;

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	worker_t t_Id;
	// thread status
	enum status t_status;
	// thread context
	ucontext_t *t_context;
	// // thread stack
	// void *t_stack;
	// thread priority
	int t_priority; //highest num is highest priority 
	// And more ...
	void *return_val;

	//Quantums (Time slices for this thread)
	int t_quantums;

	//Time values
	//struct timespec arrivalTime, responseTime, turnaroundTime, timeSinceQuantum, lastStart, lastEnd;

	long arrivalTime, responseTime, turnaroundTime, timeSinceQuantum, lastStart, lastEnd;

	//ID of thread that this thread is waiting on
	worker_t t_waitingId;


	// YOUR CODE HERE
} tcb;

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	worker_t mutex_id;
    enum mutex_status mutex_status;
    tcb* holding_thread;
	// YOUR CODE HERE
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE
//LL
typedef struct t_node{
	tcb *data;
	struct t_node *next;
} t_node;
typedef struct t_mutexNode{
	worker_mutex_t *data;
	struct t_mutexNode *next;
} t_mutexNode;
//queue
typedef struct t_queue{
	int size;
	t_node *top;
	t_node *bottom;
}t_queue;

/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

t_node* dequeue(t_queue* queue);

void enqueue(tcb* tcb, t_queue* queue);

void swap_to_scheduler();

static void schedule();

t_node* addToEndOfLinkedList(tcb* thread, t_node* list);

void alertJoinThreads();

t_node* getThread(worker_t id);

int isMutexFree(worker_mutex_t *mutex_id);

t_mutexNode* addToEndOfMutexLL(worker_mutex_t* mutex, t_mutexNode* list);

void alertMutexThreads(worker_t mutex_id);

void addToReadyQueue(tcb* curTCB, t_queue *queue);

/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

void printLL(t_node* list);

void printQueue();

tcb* dequeueMLFQ();

struct timespec getTimeDiff(struct timespec time1, struct timespec time0);

void priorityBoost();

void insertToMLFQ(tcb* thread);

void resumeTimer();

void pauseTimer();

long getMicroseconds(struct timespec timeSpec);

static void sched_mlfq();

static void sched_psjf();

void printLM(t_mutexNode* list);

void updateThreadRuntime(tcb* tcb);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif