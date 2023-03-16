// File:	thread-worker.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

struct itimerval timer;
t_queue *readyQueue;
//t_queue *blockedQueue;
tcb *currTcb;

t_node *threadsList;
t_node *blockedList;

ucontext_t* scheduler_ctx;
worker_t id = 0;
worker_t mutex_id = 0;
ucontext_t* main_ctx;

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
#define STACK_SIZE 100000

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

	// - create Thread Control Block (TCB)
	// - create and initialize the context of this worker thread
	// - allocate space of stack for this thread to run
	// after everything is set, push this thread into run queue and 
	// - make it ready for the execution.

	// YOUR CODE HERE

	//Make scheduler context if it doesn't exist yet
	if(scheduler_ctx == NULL){
		scheduler_ctx = malloc(sizeof(ucontext_t));
		getcontext(scheduler_ctx);
		scheduler_ctx->uc_link = NULL;
		scheduler_ctx->uc_stack.ss_size = STACK_SIZE;
		scheduler_ctx->uc_stack.ss_sp = malloc(STACK_SIZE);

		scheduler_ctx->uc_stack.ss_flags = 0;
		makecontext(scheduler_ctx, schedule, 0);
	}
	//Create queue of ready threads if it doesn't exist yet
	if(readyQueue == NULL) {
		readyQueue = malloc(sizeof(t_queue));
		readyQueue->size = 0;
	}

	// if(blockedQueue == NULL){
	// 	blockedQueue = malloc(sizeof(t_queue));
	// }
	
	// if(&timer == NULL){
	// 	struct sigaction sa;
	// 	memset (&sa, 0, sizeof (sa));
	// 	sa.sa_handler = &swap_to_scheduler;
	// 	sigaction (SIGPROF, &sa, NULL);

	// 	// Create timer struct
	// 	struct itimerval timer;

	// 	// Set up what the timer should reset to after the timer goes off
	// 	timer.it_interval.tv_usec = 0; 
	// 	timer.it_interval.tv_sec = 0;

	// 	// Set up the current timer to go off in 1 second
	// 	// Note: if both of the following values are zero
	// 	//       the timer will not be active, and the timer
	// 	//       will never go off even if you set the interval value
	// 	timer.it_value.tv_usec = 0;
	// 	timer.it_value.tv_sec = 1;

	// 	// Set the timer up (start the timer)
	// 	setitimer(ITIMER_PROF, &timer, NULL);	
	// }
	
	if(main_ctx == NULL) {
		tcb *main_tcb = malloc(sizeof(tcb));
		printf("Worker create: main id %d\n",id);
		main_tcb->t_Id = id++;
		main_tcb->t_status = RUNNING;
		main_tcb->t_context = malloc(sizeof(ucontext_t));
		getcontext(main_tcb->t_context);
		main_ctx = main_tcb->t_context;
		//t_node* mainNode = malloc(sizeof(t_node));
		//mainNode->data = main_tcb;
		//enqueue(main_tcb, readyQueue);
		threadsList = addToEndOfLinkedList(main_tcb, threadsList);
		currTcb = main_tcb;
	}

	//Create new thread and context
	tcb *thread_tcb = malloc(sizeof(tcb));
	printf("Worker create: thread id %d\n",id);
	thread_tcb->t_Id = id++;
	*thread = thread_tcb->t_Id;
	thread_tcb->t_status = READY;
	thread_tcb->t_priority = 0;
	thread_tcb->t_context = malloc(sizeof(ucontext_t));
	getcontext(thread_tcb->t_context);
	thread_tcb->t_context->uc_link = NULL;
	thread_tcb->t_context->uc_stack.ss_size = STACK_SIZE;
	thread_tcb->t_context->uc_stack.ss_sp = malloc(STACK_SIZE);
	thread_tcb->t_context->uc_flags = 0;
	makecontext(thread_tcb->t_context, (void *) function, 1, arg);

	//Enqueue thread to context
	enqueue(thread_tcb, readyQueue);
	//enqueue(mainNode, runQueue);
	threadsList = addToEndOfLinkedList(thread_tcb, threadsList);
	//swapcontext(main_ctx, scheduler_ctx);
    return 0;
};


/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	
	currTcb->t_status = READY;
	enqueue(currTcb, readyQueue);
	swapcontext(currTcb->t_context, scheduler_ctx);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	printf("Worker exit: Thread: %d\n", currTcb->t_Id);
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	if(value_ptr != NULL){
		currTcb->return_val = value_ptr;
	}

	alertJoinThreads();

	/*
	free(currTcb->t_context->uc_stack.ss_sp);
	free(currTcb->t_context);
	free(currTcb);
	free(currThread);
	*/
	
	currTcb->t_status = EXITED;

	printf("Worker exit end: Thread: %d\n", currTcb->t_Id);
	//switching to scheduler 
	setcontext(scheduler_ctx);

};

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	printf("Worker join: Curr Thread: %d, joining thread: %d\n", currTcb->t_Id, thread);
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	
	// YOUR CODE HERE
	
	if(getThread(thread)->data->t_status != EXITED) {
		//enqueue(currTcb, blockedQueue);
		currTcb->t_waitingId = thread;
		currTcb->t_status = BLOCKED;
		blockedList = addToEndOfLinkedList(currTcb, blockedList);
		printf("Worker join in NOT exited: Curr Thread: %d, joining thread: %d\n", currTcb->t_Id, thread);
		swapcontext(currTcb->t_context, scheduler_ctx);
	}
	if(value_ptr != NULL) {
		printf("Worker join in exited: Curr Thread: %d, joining thread: %d\n", currTcb->t_Id, thread);
		t_node* temp = threadsList;
		printLL(threadsList);
		while(temp != NULL) {
			if(temp->data->t_Id == thread) {
				*value_ptr = temp->data->return_val;
				//printf("Join value: %d", temp->data->return_val);
			}
			temp = temp->next;
		}
	}

	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex
	worker_mutex_t *curMutex = malloc(sizeof(worker_mutex_t));
    curMutex->mutex_id = mutex_id++;
    curMutex->worker_mutex_status = INITIALIZED;
    curMutex->holding_thread = currTcb;
    mutex = curMutex;
	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	return 0;
};

void printLL(t_node* list) {
	t_node* temp = list;
	while(temp != NULL) {
		printf("Threads List: %d\n", temp->data->t_Id);
		temp = temp->next;
	}
}

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// if (sched == PSJF)
	//		sched_psjf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE
	printf("Entered scheduler context\n");
	printf("Queue top: %d\n", readyQueue->top->data->t_Id);
	printf("Queue bottom: %d\n", readyQueue->bottom->data->t_Id);
	while(readyQueue->top != NULL){
		t_node* dequeuedThread = dequeue(readyQueue);
		printf("Scheduler dequeued %d\n", dequeuedThread->data->t_Id);

		currTcb = dequeuedThread->data;
		printf("Swapping to thread context %d\n", dequeuedThread->data->t_Id);
		swapcontext(scheduler_ctx, dequeuedThread->data->t_context);
		
	}
	printf("Exiting scheduler context, queue is empty\n");
	
// - schedule policy
#ifndef MLFQ
	// Choose PSJF
#else 
	// Choose MLFQ
#endif

}
/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}

// Feel free to add any other functions you need

// YOUR CODE HERE
void swap_to_scheduler(){
	ucontext_t *curr;
	getcontext(curr);
	swapcontext(curr,scheduler_ctx);
}

void enqueue(tcb* tcb, t_queue* queue) {
	t_node* newNode = malloc(sizeof(t_node));
	newNode->data = tcb;

	//If queue is empty, set top and bottom to the new node
	if(queue->top == NULL) {
		queue->top = newNode;
		queue->bottom = newNode;
	}
	//Else, add to end of queue (bottom->next) and set bottom to new node
	else{
		queue->bottom->next = newNode;
		queue->bottom = newNode;
	}
	queue->size++;
}

t_node* dequeue(t_queue* queue) {
	if(queue->top == NULL) {
        return NULL;
    }

    t_node* temp = queue->top;
    queue->top = queue->top->next;
    if(queue->top == NULL) {
        queue->bottom = NULL; // queue is now empty
    }
    temp->next = NULL;
	queue->size--;
    return temp;
}

t_node* addToEndOfLinkedList(tcb* tcb, t_node* list){
	t_node* newNode = malloc(sizeof(t_node));
	newNode->data = tcb;

	if(list == NULL){
		list = newNode;
		return list;
	}

	t_node* temp = list;
	while(temp->next != NULL) {
		temp = temp->next;
	}
	temp->next = newNode;
	return list;
}

void printQueue() {
	t_node* temp = readyQueue->top;
	if(readyQueue->top == NULL) {
		return;
	}
	while(temp->next != NULL) {
		printf("Ready Queue: %d\n", temp->data->t_Id);
		temp = temp->next;
	}
}

void alertJoinThreads() {
	t_node* temp = threadsList;
	while(temp != NULL){
		if(temp->data->t_waitingId == currTcb->t_Id) {
			temp->data->t_waitingId = -1;
			temp->data->t_status = READY;
			enqueue(temp->data, readyQueue);
		}
		temp = temp->next;
	}
}

t_node* getThread(worker_t id) {
	t_node* temp = threadsList;
	while(temp != NULL) {
		if(temp->data->t_Id == id) {
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}