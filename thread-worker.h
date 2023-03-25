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

#define QUANTUM 10000					// Quantum slice time in microseconds
#define PRIORITY_BOOST_TIME 50000		// Time "S" before priority boost should happen
#define MLFQ_QUEUES_NUM 3				// Number of queues in MLFQ

typedef uint worker_t;

enum status {
	RUNNING,
	READY,
	BLOCKED_JOIN,
	BLOCKED_MUTEX,
	EXITED
} status;
enum mutex_status {
	INITIALIZED,
	LOCKED,
	UNLOCKED
} mutex_status;

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	worker_t t_Id;
	// thread status
	enum status t_status;
	// thread context
	ucontext_t *t_context;
	// thread priority (higher number = higher priority)
	int t_priority;
	// And more ...
	void *return_val;

	//Quantums (Number of time slices completed for this thread)
	unsigned int t_quantums;

	//Benchmarking statistics values
	struct timespec arrivalTime, responseTime, turnaroundTime;

	//Amount of quantum used since the last time thread was scheduled
	unsigned int amount_quantum_used;

	//Time when thread was last scheduled
	struct timespec last_scheduled;

	//ID of thread or mutex that this thread is waiting on
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

//Queue
typedef struct t_queue{
	int size;
	t_node *top;
	t_node *bottom;
} t_queue;

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

/* Selects which type of scheduler to use (MLFQ or PSJF). */
static void schedule();

/* PSJF Scheduler. */
static void sched_psjf();

/* MLFQ Scheduler. */
static void sched_mlfq();

/*
Function called when timer goes off. Adjusts quantum and
priority information for current thread.
*/
void swap_to_scheduler();

/* Enqueue tcb to queue */
void enqueue(tcb* tcb, t_queue* queue);

/* Dequeue tcb from queue */
tcb* dequeue(t_queue* queue);

/* Dequeues tcb from MLFQ. */
tcb* dequeueMLFQ();

/* Adds tcb to end of linked list. */
t_node* addToEndOfLinkedList(tcb* thread, t_node* list);

/* Adds mutex to end of list. */
t_mutexNode* addToEndOfMutexLL(worker_mutex_t* mutex, t_mutexNode* list);

/* Unblocks threads that are joined on the current tcb. */
void alertJoinThreads();

/* Unblocks threads that are mutex locked by the current tcb. */
void alertMutexThreads(worker_t mutex_id);

/* Returns thread given it's id. */
t_node* getThread(worker_t id);

/* Checks if mutex is free. Returns 1 if free, 0 if not free. */
int isMutexFree(worker_mutex_t *mutex_id);

/* Adds tcb to ready queue.*/
void addToReadyQueue(tcb* curTCB, t_queue *queue);

/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

/* Sets priority of all threads to the highest priority.*/
void priorityBoost();

/* Set timer to run for time 'remaining'. */
void setTimer(int remaining);

/* Convert timespec to microseconds. */
double getMicroseconds(struct timespec timeSpec);

/* Gets the difference between two timespecs. */
struct timespec diff_timespec(struct timespec endTime, struct timespec startTime);

/* Prints linked list. */
void printLL(t_node* list);

/* Prints mutex list. */
void printLM(t_mutexNode* list);

/* Prints queue. */
void printQueue(t_queue* queue);

/*exit function to free everything*/
void exitFunction();

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