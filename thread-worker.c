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

ucontext_t* scheduler_ctx;


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
		//getcontext(scheduler_ctx);
		void *schedulerSt = malloc(STACK_SIZE);
		scheduler_ctx->uc_link = NULL;
		scheduler_ctx->uc_stack.ss_sp = schedulerSt;
		scheduler_ctx->uc_stack.ss_size = STACK_SIZE;
		scheduler_ctx->uc_stack.ss_flags = 0;
		makecontext(scheduler_ctx, schedule,NULL);
	}
	if(runQueue == NULL) {
		runQueue = malloc(sizeof(t_queue));
	}

	if(blockedQueue == NULL){
		blockedQueue = malloc(sizeof(t_queue));
	}
	
	if(&timer == NULL){
		struct sigaction sa;
		memset (&sa, 0, sizeof (sa));
		sa.sa_handler = &swap_to_scheduler;
		sigaction (SIGPROF, &sa, NULL);

		// Create timer struct
		struct itimerval timer;

		// Set up what the timer should reset to after the timer goes off
		timer.it_interval.tv_usec = 0; 
		timer.it_interval.tv_sec = 0;

		// Set up the current timer to go off in 1 second
		// Note: if both of the following values are zero
		//       the timer will not be active, and the timer
		//       will never go off even if you set the interval value
		timer.it_value.tv_usec = 0;
		timer.it_value.tv_sec = 1;

		// Set the timer up (start the timer)
		setitimer(ITIMER_PROF, &timer, NULL);	
	}
	
	
	tcb *main_thread = malloc(sizeof(tcb));
	main_thread->t_Id = 0;
	main_thread->t_status = RUNNING;
	ucontext_t *main_thread_ctx = malloc(sizeof(ucontext_t));
	getcontext(main_thread_ctx);
	main_thread->t_context = main_thread_ctx;
	t_node* node = malloc(sizeof(t_node));
	node->data = main_thread;
	enqueue(node, runQueue);



	tcb *thread_tcb = malloc(sizeof(tcb));
	thread_tcb->t_Id = *thread;
	thread_tcb->t_status = READY;
	thread_tcb->t_priority = 0;
	ucontext_t *thread_ctx = malloc(sizeof(ucontext_t));
	//getcontext(thread_ctx);
	void *st = malloc(STACK_SIZE);
	thread_ctx->uc_link = NULL;
	thread_ctx->uc_stack.ss_sp = st;
	thread_ctx->uc_stack.ss_size = STACK_SIZE;
	thread_ctx->uc_stack.ss_flags = 0;
	makecontext(thread_ctx,*function,arg);
	thread_tcb->t_context = thread_ctx;
	thread_tcb->t_stack = st;
	t_node* node = malloc(sizeof(t_node));
	node->data = thread_tcb;
	enqueue(node, runQueue);

    return 0;
};

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

void swap_to_scheduler(){
	ucontext_t *curr;
	getcontext(curr);
	swapcontext(curr,scheduler_ctx);
}


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
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	if(value_ptr != NULL){
		value_ptr = currThread->data->return_val;
	}
	free(currThread->data->t_context->uc_stack.ss_sp);
	free(currThread->data->t_context);
	free(currThread->data->t_stack);
	free(currThread->data);
	free(currThread);
	//switching to scheduler context
	setcontext(scheduler_ctx);

};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	
	// YOUR CODE HERE
	
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

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

