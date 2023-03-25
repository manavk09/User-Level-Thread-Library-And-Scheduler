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

bool isTimerCreated = false;
struct itimerval timer;

uint priorityBoostCounter = 0;
uint threadsExited = 0;

tcb *currTcb;

t_queue *readyQueue;
t_queue* mlfq[MLFQ_QUEUES_NUM];

t_node *threadsList;
t_mutexNode *mutex_list;

worker_t id = 0;
worker_t mutex_id = 0;

ucontext_t* main_ctx;
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
		getcontext(scheduler_ctx);
		scheduler_ctx->uc_link = NULL;
		scheduler_ctx->uc_stack.ss_size = STACK_SIZE;
		scheduler_ctx->uc_stack.ss_sp = malloc(STACK_SIZE);

		scheduler_ctx->uc_stack.ss_flags = 0;
		makecontext(scheduler_ctx, schedule, 0);
	}

	#ifdef MLFQ
		//Create MLFQ if it doesn't exist yet
		if(mlfq[0] == NULL) {
			for(int i = 0; i < MLFQ_QUEUES_NUM; i++) {
				mlfq[i] = malloc(sizeof(t_queue));
			}
		}
	#else
		//Create queue of ready threads if it doesn't exist yet
		if(readyQueue == NULL) {
			readyQueue = malloc(sizeof(t_queue));
			readyQueue->size = 0;
		}
	#endif
	
	if(main_ctx == NULL) {
		tcb *main_tcb = malloc(sizeof(tcb));
		main_tcb->t_Id = id++;
		main_tcb->t_status = RUNNING;
		main_tcb->t_context = malloc(sizeof(ucontext_t));
		getcontext(main_tcb->t_context);
		main_ctx = main_tcb->t_context;
		main_tcb->t_priority = MLFQ_QUEUES_NUM - 1;
		main_tcb->t_quantums = 0;
		clock_gettime(CLOCK_REALTIME, &(main_tcb->arrivalTime));
		threadsList = addToEndOfLinkedList(main_tcb, threadsList);
		currTcb = main_tcb;
	}

	//Create new thread and context
	tcb *thread_tcb = malloc(sizeof(tcb));
	thread_tcb->t_Id = id++;
	thread_tcb->t_quantums = 0;
	*thread = thread_tcb->t_Id;
	thread_tcb->t_status = READY;
	thread_tcb->t_priority = MLFQ_QUEUES_NUM - 1;
	thread_tcb->t_context = malloc(sizeof(ucontext_t));
	getcontext(thread_tcb->t_context);
	thread_tcb->t_context->uc_link = NULL;
	thread_tcb->t_context->uc_stack.ss_size = STACK_SIZE;
	thread_tcb->t_context->uc_stack.ss_sp = malloc(STACK_SIZE);
	thread_tcb->t_context->uc_flags = 0;
	makecontext(thread_tcb->t_context, (void *) function, 1, arg);

	//Set arrival time of this new thread
	clock_gettime(CLOCK_REALTIME, &(thread_tcb->arrivalTime));
	
	//Enqueue new thread
	#ifdef MLFQ
		enqueue(thread_tcb, mlfq[thread_tcb->t_priority]);
	#else
		addToReadyQueue(thread_tcb, readyQueue);
	#endif
	
	threadsList = addToEndOfLinkedList(thread_tcb, threadsList);

	if(!isTimerCreated) {
		isTimerCreated = true;
		struct sigaction sa;
		memset (&sa, 0, sizeof (sa));
		sa.sa_handler = &swap_to_scheduler;
		sigaction (SIGPROF, &sa, NULL);
	}
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE

	//Block timer signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPROF);
	sigprocmask(SIG_SETMASK, &set, NULL);

	struct timespec curr_time;
	clock_gettime(CLOCK_REALTIME, &curr_time);

	currTcb->amount_quantum_used += (curr_time.tv_sec - currTcb->last_scheduled.tv_sec) * 1000000 + 
									(curr_time.tv_nsec - currTcb->last_scheduled.tv_nsec) / 1000;
	
	if(currTcb->amount_quantum_used >= QUANTUM){
		currTcb->t_quantums++;
		currTcb->amount_quantum_used = 0;

		priorityBoostCounter++;
		currTcb->t_priority == 0 ? currTcb->t_priority = 0 : currTcb->t_priority--;
	}

	currTcb->t_status = READY;

	tot_cntx_switches++;
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	swapcontext(currTcb->t_context, scheduler_ctx);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	//Block timer signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPROF);
	sigprocmask(SIG_SETMASK, &set, NULL);

	//Set turnaround time
	struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &(currentTime));
    currTcb->turnaroundTime = diff_timespec(currentTime, currTcb->arrivalTime);

	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	if(value_ptr != NULL) {
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
	threadsExited++;

	//Adjust bechmarking statistics
	avg_resp_time = (avg_resp_time * (threadsExited - 1) + getMicroseconds(currTcb->responseTime)) / threadsExited;
	avg_turn_time = (avg_turn_time * (threadsExited - 1) + getMicroseconds(currTcb->turnaroundTime)) / threadsExited;
	
	//switching to scheduler 
	tot_cntx_switches++;
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	setcontext(scheduler_ctx);

};

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	//Block timer signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPROF);
	sigprocmask(SIG_SETMASK, &set, NULL);

	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	
	// YOUR CODE HERE
	
	if(getThread(thread)->data->t_status != EXITED) {
		currTcb->t_waitingId = thread;
		currTcb->t_status = BLOCKED_JOIN;

		struct timespec curr_time;
		clock_gettime(CLOCK_REALTIME, &curr_time);

		currTcb->amount_quantum_used += (curr_time.tv_sec - currTcb->last_scheduled.tv_sec) * 1000000 + 
										(curr_time.tv_nsec - currTcb->last_scheduled.tv_nsec) / 1000;
		
		if(currTcb->amount_quantum_used >= QUANTUM){
			currTcb->t_quantums++;
			currTcb->amount_quantum_used = 0;

			priorityBoostCounter++;
			currTcb->t_priority == 0 ? currTcb->t_priority = 0 : currTcb->t_priority--;
		}

		tot_cntx_switches++;
		swapcontext(currTcb->t_context, scheduler_ctx);
	}
	if(value_ptr != NULL) {
		t_node* temp = threadsList;
		while(temp != NULL) {
			if(temp->data->t_Id == thread) {
				*value_ptr = temp->data->return_val;
			}
			temp = temp->next;
		}
	}
	sigprocmask(SIG_UNBLOCK, &set, NULL);

	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//Block timer signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPROF);
	sigprocmask(SIG_SETMASK, &set, NULL);

	//- initialize data structures for this mutex
	worker_mutex_t *curMutex = malloc(sizeof(worker_mutex_t));
    curMutex->mutex_id = mutex_id++;
    curMutex->mutex_status = INITIALIZED;
	mutex_list = addToEndOfMutexLL(curMutex,mutex_list);
    mutex = curMutex;
	// YOUR CODE HERE

	sigprocmask(SIG_UNBLOCK, &set, NULL);
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

		//Block timer signal
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGPROF);
		sigprocmask(SIG_SETMASK, &set, NULL);

		while(isMutexFree(mutex) != 1){
			currTcb->t_status = BLOCKED_MUTEX;
			currTcb->t_waitingId = mutex->mutex_id;

			struct timespec curr_time;
			clock_gettime(CLOCK_REALTIME, &curr_time);

			currTcb->amount_quantum_used += (curr_time.tv_sec - currTcb->last_scheduled.tv_sec) * 1000000 + 
											(curr_time.tv_nsec - currTcb->last_scheduled.tv_nsec) / 1000;
			
			if(currTcb->amount_quantum_used >= QUANTUM){
				currTcb->t_quantums++;
				currTcb->amount_quantum_used = 0;

				priorityBoostCounter++;
				currTcb->t_priority == 0 ? currTcb->t_priority = 0 : currTcb->t_priority--;
			}

			tot_cntx_switches++;
			swapcontext(currTcb->t_context,scheduler_ctx);
		}
		mutex->mutex_status = LOCKED;
		mutex->holding_thread = currTcb;

		sigprocmask(SIG_UNBLOCK, &set, NULL);
        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.
	/*
		go to it in mutex list unlock
		alert all waiting thread
	*/
	// YOUR CODE HERE

	//Block timer signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPROF);
	sigprocmask(SIG_SETMASK, &set, NULL);

	if(currTcb->t_Id == mutex->holding_thread->t_Id){
		mutex->mutex_status = UNLOCKED;
		mutex->holding_thread = NULL;
		alertMutexThreads(mutex->mutex_id);
	}
	
	sigprocmask(SIG_UNBLOCK, &set, NULL);

	return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
	//printf("we in destroy\n");

	//Block timer signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPROF);
	sigprocmask(SIG_SETMASK, &set, NULL);

	t_mutexNode* curr = mutex_list;
	//t_mutexNode* prev = mutex_list;
	if(curr->data->mutex_id == mutex->mutex_id){
		mutex_list = curr->next;
		//doo the free stuff
		//free(curr);
		//resumeTimer();
		return 0;
	}
	while(curr->next != NULL){
		if(curr->next->data->mutex_id == mutex->mutex_id){
			t_mutexNode* temp = curr->next;
			curr->next = curr->next->next;
			//free the stuff
			//free(temp);
			break;
		}
		else{
			curr = curr->next;
		}
	}

	sigprocmask(SIG_UNBLOCK, &set, NULL);
	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// YOUR CODE HERE
	
// - schedule policy
#ifndef MLFQ
	// Choose PSJF
	sched_psjf();
#else 
	// Choose MLFQ
	sched_mlfq();
#endif

}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
    while(1) {
		//Block timer signal
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGPROF);
		sigprocmask(SIG_SETMASK, &set, NULL);
		if(currTcb->t_status == READY){
			addToReadyQueue(currTcb,readyQueue);
		}

        tcb* dequeuedThread = dequeue(readyQueue);
		if(dequeuedThread->responseTime.tv_nsec == 0 && dequeuedThread->responseTime.tv_sec == 0) {
			struct timespec currentTime;
			clock_gettime(CLOCK_REALTIME, &(currentTime));
			dequeuedThread->responseTime = diff_timespec(currentTime, dequeuedThread->arrivalTime);
		}

        currTcb = dequeuedThread;
		tot_cntx_switches++;

		sigprocmask(SIG_UNBLOCK, &set, NULL);
		setTimer(dequeuedThread->amount_quantum_used);
		clock_gettime(CLOCK_REALTIME, &(dequeuedThread->last_scheduled));
		
        swapcontext(scheduler_ctx, dequeuedThread->t_context);
    }
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	
	while(1) {
		//Block timer signal
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGPROF);
		sigprocmask(SIG_SETMASK, &set, NULL);

		if(currTcb->t_status == READY) {
			enqueue(currTcb, mlfq[currTcb->t_priority]);
		}

		if(priorityBoostCounter >= PRIORITY_BOOST_TIME/QUANTUM) {
			priorityBoost();
			priorityBoostCounter = 0;
		}

		tcb* dequeuedThread = dequeueMLFQ();
		
		if(dequeuedThread->responseTime.tv_nsec == 0 && dequeuedThread->responseTime.tv_sec == 0) {
			struct timespec currentTime;
			clock_gettime(CLOCK_REALTIME, &(currentTime));
			dequeuedThread->responseTime = diff_timespec(currentTime, dequeuedThread->arrivalTime);
		}

        currTcb = dequeuedThread;
		tot_cntx_switches++;

		sigprocmask(SIG_UNBLOCK, &set, NULL);
		setTimer(dequeuedThread->amount_quantum_used);
		clock_gettime(CLOCK_REALTIME, &(dequeuedThread->last_scheduled));

		swapcontext(scheduler_ctx, dequeuedThread->t_context);		
	}

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
	//printf("Timer ring, in swap_to_scheduler\n");

	currTcb->t_quantums++;
	currTcb->amount_quantum_used = 0;

	priorityBoostCounter++;
	currTcb->t_priority == 0 ? currTcb->t_priority = 0 : currTcb->t_priority--;

	tot_cntx_switches++;
	swapcontext(currTcb->t_context, scheduler_ctx);
}

void enqueue(tcb* tcb, t_queue* queue) {
	t_node* newNode = malloc(sizeof(t_node));
	newNode->data = tcb;
	newNode->next = NULL;
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

tcb* dequeue(t_queue* queue) {
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
    return temp->data;
}

tcb* dequeueMLFQ() {
	int currLevel = MLFQ_QUEUES_NUM - 1;

	for(currLevel; currLevel >= 0; currLevel--) {
		if(mlfq[currLevel]->size != 0) {
			return dequeue(mlfq[currLevel]);
		}
	}
	return NULL;
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

t_mutexNode* addToEndOfMutexLL(worker_mutex_t* mutex, t_mutexNode* list){
	t_mutexNode* newNode = malloc(sizeof(t_mutexNode));
	newNode->data = mutex;
	if(list == NULL){
		list = newNode;
		return list;
	}

	t_mutexNode* temp = list;
	while(temp->next != NULL) {
		temp = temp->next;
	}
	temp->next = newNode;
	return list;
}

void alertJoinThreads() {
	t_node* temp = threadsList;
	while(temp != NULL){
		if(temp->data->t_waitingId == currTcb->t_Id && temp->data->t_status == BLOCKED_JOIN) {
			temp->data->t_waitingId = -1;
			temp->data->t_status = READY;
			#ifndef MLFQ
				// Choose PSJF
				addToReadyQueue(temp->data, readyQueue);
			#else 
				// Choose MLFQ
				enqueue(temp->data, mlfq[temp->data->t_priority]);

			#endif
			
		}
		temp = temp->next;
	}

}

void alertMutexThreads(worker_t mutex_id) {

	t_node* temp = threadsList;
	while(temp != NULL){
		if(temp->data->t_waitingId == mutex_id && temp->data->t_status == BLOCKED_MUTEX) {
			temp->data->t_waitingId = -1;
			temp->data->t_status = READY;
			#ifndef MLFQ
				// Choose PSJF
				addToReadyQueue(temp->data, readyQueue);
			#else 
				// Choose MLFQ
				enqueue(temp->data, mlfq[temp->data->t_priority]);
			#endif
			
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

int isMutexFree(worker_mutex_t *mutex){
	if(mutex->mutex_status == UNLOCKED || mutex->mutex_status == INITIALIZED){
		return 1; //it is free
	}
	else{
		return 0; //it is locked
	}
}

void addToReadyQueue(tcb* curTCB, t_queue* queue){
	t_node* curNode = malloc(sizeof(t_node));
	curNode->data = curTCB;
	curNode->next = NULL;

	//if queue is empty
	if(queue->top == NULL) {
		queue->top = curNode;
		queue->bottom = curNode;
	}
	else{
	//inserting to the top of queue
		if(curNode->data->t_quantums <= queue->top->data->t_quantums){
			curNode->next = queue->top;
			queue->top = curNode;
		}
		else{
			t_node* temp = queue->top;
			while(temp->next != NULL && temp->next->data->t_quantums < curNode->data->t_quantums){
				temp = temp->next;
			}

			curNode->next = temp->next;
			temp->next = curNode;
			if(queue->bottom == temp){
				queue->bottom = curNode;
			}
		}
	}
	queue->size++;	
}

void priorityBoost() {
	int currLevel = MLFQ_QUEUES_NUM - 2;

	for(currLevel; currLevel >= 0; currLevel--) {
		while(mlfq[currLevel]->size != 0) {
			tcb* curr = dequeue(mlfq[currLevel]);
			curr->t_priority = MLFQ_QUEUES_NUM - 1;
			enqueue(curr, mlfq[MLFQ_QUEUES_NUM - 1]);
		}
	}
}

void setTimer(int remaining){
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM - remaining;

	setitimer(ITIMER_PROF, &timer, NULL);
}

double getMicroseconds(struct timespec timeSpec) {
	return timeSpec.tv_sec*1000000 + timeSpec.tv_nsec/(double)1000;
}

struct timespec diff_timespec(struct timespec endTime, struct timespec startTime) {
    struct timespec diffTime;
    if(endTime.tv_nsec - startTime.tv_nsec < 0) {
        diffTime.tv_sec = endTime.tv_sec - startTime.tv_sec - 1;
        diffTime.tv_nsec = 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
    }
    else {
        diffTime.tv_sec = endTime.tv_sec - startTime.tv_sec;
        diffTime.tv_nsec = endTime.tv_nsec - startTime.tv_nsec;
    }
    return diffTime;
}

void printLL(t_node* list) {
	t_node* temp = list;
	printf("Printing threads list\n");
	while(temp != NULL) {
		printf("Threads List: %d with status %d\n", temp->data->t_Id, temp->data->t_status);
		temp = temp->next;
	}
}

void printLM(t_mutexNode* list) {
	t_mutexNode* temp = list;
	while(temp != NULL) {
		printf("MutexList: %d\n", temp->data->mutex_id);
		temp = temp->next;
	}
}

void printQueue(t_queue* queue) {
	printf("Printing Queue Size: %d\n", queue->size);
	t_node* temp = queue->top;
	if(queue->top == NULL) {
		return;
	}
	while(temp != NULL) {
		printf("Printing Queue: Thread: %d with status %d with priority %d\n", temp->data->t_Id, temp->data->t_status, temp->data->t_priority);
		temp = temp->next;
	}
}