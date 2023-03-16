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
t_queue *runQueue;
t_queue *blockedQueue;
t_node *currThread;

t_node *threadsList;
t_node *locked_mutexlst;

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
		makecontext(scheduler_ctx,schedule,0);
	}
	if(runQueue == NULL) {
		runQueue = malloc(sizeof(t_queue));
	}

	if(blockedQueue == NULL){
		blockedQueue = malloc(sizeof(t_queue));
	}
	
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
		tcb *main_thread = malloc(sizeof(tcb));
		printf("Worker create: main id %d\n",id);
		main_thread->t_Id = id++;
		main_thread->t_status = RUNNING;
		main_thread->t_context = malloc(sizeof(ucontext_t));
		getcontext(main_thread->t_context);
		main_ctx = main_thread->t_context;
		t_node* mainNode = malloc(sizeof(t_node));
		mainNode->data = main_thread;
		enqueue(mainNode, runQueue);
		addToEndOfLinkedList(mainNode);
		currThread = mainNode;
	}

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
	makecontext(thread_tcb->t_context,(void *)function,1,arg);

	t_node* thread_node = malloc(sizeof(t_node));
	thread_node->data = thread_tcb;
	enqueue(thread_node, runQueue);
	//enqueue(mainNode, runQueue);
	addToEndOfLinkedList(thread_node);
	//swapcontext(main_ctx, scheduler_ctx);
    return 0;
};


/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	
	currThread->data->t_status = READY;
	swapcontext(currThread->data->t_context, scheduler_ctx);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	printf("Worker exit: Thread: %d\n", currThread->data->t_Id);
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	if(value_ptr != NULL){
		value_ptr = currThread->data->return_val;
	}

	alertJoinThreads();

	/*
	free(currThread->data->t_context->uc_stack.ss_sp);
	free(currThread->data->t_context);
	free(currThread->data);
	free(currThread);
	*/
	
	currThread->data->t_status = EXITED;

	printf("Worker exit end: Thread: %d\n", currThread->data->t_Id);
	//switching to scheduler 
	setcontext(scheduler_ctx);

};

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	printf("Thread: %d\n", thread);
	printf("Worker join: Curr Thread: %d, joining thread: %d\n", currThread->data->t_Id, thread);
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	
	// YOUR CODE HERE
	
	if(getThread(thread)->data->t_status != EXITED) {
		enqueue(currThread, blockedQueue);
		currThread->data->t_waitingId = thread;
		currThread->data->t_status = BLOCKED;
		printf("Worker join in not exited: Curr Thread: %d, joining thread: %d\n", currThread->data->t_Id, thread);
		swapcontext(currThread->data->t_context, scheduler_ctx);
	}
	else {
		if(value_ptr != NULL) {
			printf("Worker join in exited: Curr Thread: %d, joining thread: %d\n", currThread->data->t_Id, thread);
			t_node* temp = threadsList;
			while(temp != NULL) {
				if(temp->data->t_Id == thread) {
					value_ptr = temp->data->return_val;
				}
				temp = temp->next;
			}
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
	curMutex->holding_thread = currThread->data;
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
		int found = search(mutex);
		if(found == 1){//already locked by some other thread so block our cur thread
			currThread->data->t_status = BLOCKED;
		}
		else{
			mutex->holding_thread = currThread->data;
			mutex->worker_mutex_status = LOCKED;
		}

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
	printf("%d\n", runQueue->top->data->t_Id);
	printf("%d\n", runQueue->bottom->data->t_Id);

	while(runQueue->top != NULL){
		t_node* dequeuedThread = dequeue(runQueue);
		if(dequeuedThread->data->t_status == BLOCKED) {
			enqueue(dequeuedThread, runQueue);
		}
		else {
			currThread = dequeuedThread;
			printf("Boutta swap sche\n");
			swapcontext(scheduler_ctx, dequeuedThread->data->t_context);
		}
		
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
void enqueue(t_node* node, t_queue* queue) {
	if(queue->top == NULL) {
		queue->top = node;
		queue->bottom = node;
	}
	else{
		queue->bottom->next = node;
		queue->bottom = node;
	}
}

t_node* dequeue(t_queue* queue) {
	if(queue->top == NULL) {
        return NULL; // queue is empty
    }
    t_node* temp = queue->top;
    queue->top = queue->top->next;
    if(queue->top == NULL) {
        queue->bottom = NULL; // queue is now empty
    }
    temp->next = NULL;
    return temp;
}

void addToEndOfLinkedList(t_node* thread){
	if(threadsList == NULL){
		threadsList = thread;
		return;
	}
	
	t_node* temp = threadsList;
	while(temp->next != NULL) {
		temp = temp->next;
	}
	temp->next = thread;

}

void alertJoinThreads() {
	t_node* temp = threadsList;
	while(temp != NULL){
		printf("ggsdfg\n");
		if(temp->data->t_waitingId == currThread->data->t_Id) {
			temp->data->t_waitingId = -1;
			temp->data->t_status = READY;
			enqueue(temp, runQueue);
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