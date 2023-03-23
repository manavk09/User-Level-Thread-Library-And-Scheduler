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
struct itimerval zero_timer = {0};
uint priorityBoostTime = 0;
uint threadsExited = 0;
struct timespec timerLastStarted;
bool isTimerCreated = false;

bool didTimerRing = false;

t_queue *readyQueue;
tcb *currTcb;

t_queue* mlfq[MLFQ_QUEUES_NUM];

t_node *threadsList;
t_node *blockedList;
t_mutexNode *mutex_list;

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
		//printf("Sched context created\n");
	}

	//Create queue of ready threads if it doesn't exist yet
	if(readyQueue == NULL) {
		readyQueue = malloc(sizeof(t_queue));
		readyQueue->size = 0;
	}

	//Create MLFQ if it doesn't exist yet
	if(mlfq[0] == NULL) {
		for(int i = 0; i < MLFQ_QUEUES_NUM; i++) {
			//printf("Mallocing MLFQ\n");
			mlfq[i] = malloc(sizeof(t_queue));
		}
	}
	
	if(main_ctx == NULL) {
		tcb *main_tcb = malloc(sizeof(tcb));
		//printf("Worker create: main id %d\n",id);
		main_tcb->t_Id = id++;
		main_tcb->t_status = RUNNING;
		main_tcb->t_context = malloc(sizeof(ucontext_t));
		getcontext(main_tcb->t_context);
		main_ctx = main_tcb->t_context;
		main_tcb->t_priority = MLFQ_QUEUES_NUM - 1;
		main_tcb->t_quantums = 0;
		//main_tcb->timeSinceQuantum = 0;

		//struct timespec currentTime;
		//clock_gettime(CLOCK_REALTIME, &(currentTime));
		//main_tcb->arrivalTime = getMicroseconds(currentTime);
		clock_gettime(CLOCK_REALTIME, &(main_tcb->arrivalTime));

		threadsList = addToEndOfLinkedList(main_tcb, threadsList);
		currTcb = main_tcb;
		clock_gettime(CLOCK_REALTIME, &(main_tcb->lastStart));
		//printf("main tc created: %d\n",currTcb->t_Id);
	}

	//Create new thread and context
	tcb *thread_tcb = malloc(sizeof(tcb));
	//printf("Worker create: thread id %d\n",id);
	thread_tcb->t_Id = id++;
	thread_tcb->t_quantums = 0;
	//thread_tcb->timeSinceQuantum = 0;
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
	// struct timespec currentTime;
	// clock_gettime(CLOCK_REALTIME, &(currentTime));
	// thread_tcb->arrivalTime = getMicroseconds(currentTime);
	clock_gettime(CLOCK_REALTIME, &(thread_tcb->arrivalTime));


	//Enqueue thread to context
	//enqueue(thread_tcb, readyQueue);
	#ifdef MLFQ
		insertToMLFQ(thread_tcb);
	#else
		addToReadyQueue(thread_tcb, readyQueue);
	#endif
	//printQueue(readyQueue);
	//printf("done printing q\n");
	//enqueue(mainNode, runQueue);
	threadsList = addToEndOfLinkedList(thread_tcb, threadsList);
	//swapcontext(main_ctx, scheduler_ctx);
	if(!isTimerCreated) {
		isTimerCreated = true;
		//printf("Making timer\n");
		struct sigaction sa;
		memset (&sa, 0, sizeof (sa));
		sa.sa_handler = &swap_to_scheduler;
		sigaction (SIGPROF, &sa, NULL);

		// Set up what the timer should reset to after the timer goes off
		timer.it_interval.tv_usec = 0; 
		timer.it_interval.tv_sec = 0;

		// Set up the current timer to go off in 1 second
		// Note: if both of the following values are zero
		//       the timer will not be active, and the timer
		//       will never go off even if you set the interval value
		timer.it_value.tv_usec = QUANTUM;
		timer.it_value.tv_sec = 0;

		// Set the timer up (start the timer)
		clock_gettime(CLOCK_REALTIME, &timerLastStarted);
		setitimer(ITIMER_PROF, &timer, NULL);
	}
    return 0;
};


/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE

	//Pause Timer
	pauseTimer();

	//Update runtime of thread (WITH LONGS);
	// struct timespec currentTime;
	// clock_gettime(CLOCK_REALTIME, &(currentTime));
	// currTcb->lastEnd = getMicroseconds(currentTime);
	// long lastRunTime = currTcb->lastEnd - currTcb->lastStart;
	// currTcb->timeSinceQuantum += lastRunTime;

	currTcb->t_status = READY;
	//enqueue(currTcb, readyQueue);

	tot_cntx_switches++;
	swapcontext(currTcb->t_context, scheduler_ctx);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	//Pause Timer
	pauseTimer();

	//Set turnaround time
	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &(currentTime));
	currTcb->turnaroundTime = diff_timespec(currentTime, currTcb->arrivalTime);
	//printf("within exit now: %d\n", currTcb->t_Id);
	//Set turnaround time
	// struct timespec currentTime;
	// clock_gettime(CLOCK_REALTIME, &(currentTime));
	// currTcb->turnaroundTime = getMicroseconds(currentTime) - currTcb->arrivalTime;

	//printf("Worker exit: Thread: %d\n", currTcb->t_Id);
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

	//("Worker exit end: Thread: %d\n", currTcb->t_Id);

	//Adjust bechmarking statistics (once main is finished)
	//printf("Thread %d turnaround time: %ld\n", currTcb->t_Id, currTcb->turnaroundTime);
	// avg_resp_time += currTcb->responseTime;
	// avg_turn_time += currTcb->turnaroundTime;

	// avg_resp_time = (avg_resp_time * (threadsExited - 1) + currTcb->responseTime) / threadsExited;
	// avg_turn_time = (avg_turn_time * (threadsExited - 1) + currTcb->turnaroundTime) / threadsExited;
	avg_resp_time = (avg_resp_time * (threadsExited - 1) + getMicroseconds(currTcb->responseTime)) / threadsExited;
	avg_turn_time = (avg_turn_time * (threadsExited - 1) + getMicroseconds(currTcb->turnaroundTime)) / threadsExited;
	
	//switching to scheduler 
	tot_cntx_switches++;
	//printf("thread that is about to finish: %d\n",currTcb->t_Id);
	setcontext(scheduler_ctx);

};

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	//Pause Timer
	pauseTimer();

	printf("Worker join: Curr Thread: %d, joining thread: %d\n", currTcb->t_Id, thread);
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	
	// YOUR CODE HERE
	
	if(getThread(thread)->data->t_status != EXITED) {
		//enqueue(currTcb, blockedQueue);
		currTcb->t_waitingId = thread;
		currTcb->t_status = BLOCKED_JOIN;
		blockedList = addToEndOfLinkedList(currTcb, blockedList);
		//printf("Worker join in NOT exited: Curr Thread: %d, joining thread: %d\n", currTcb->t_Id, thread);
		tot_cntx_switches++;
		swapcontext(currTcb->t_context, scheduler_ctx);
	}
	if(value_ptr != NULL) {
		//printf("Worker join in exited: Curr Thread: %d, joining thread: %d\n", currTcb->t_Id, thread);
		t_node* temp = threadsList;
		//printLL(threadsList);
		while(temp != NULL) {
			if(temp->data->t_Id == thread) {
				*value_ptr = temp->data->return_val;
				// printf("Join value: %d\n", temp->data->return_val);
			}
			temp = temp->next;
		}
	}

	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//Pause Timer
	pauseTimer();
	//- initialize data structures for this mutex
	worker_mutex_t *curMutex = malloc(sizeof(worker_mutex_t));
    curMutex->mutex_id = mutex_id++;
    curMutex->mutex_status = INITIALIZED;
    //curMutex->holding_thread = currTcb;
	mutex_list = addToEndOfMutexLL(curMutex,mutex_list);
    mutex = curMutex;
	//printf("in mutex init\n");
	//printLM(mutex_list);
	// YOUR CODE HERE
	resumeTimer();
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

		//Pause Timer
		pauseTimer();
		printf("Mutex Lock: Thread: %d\n", currTcb->t_Id);

		while(isMutexFree(mutex) != 1){
			currTcb->t_status = BLOCKED_MUTEX;
			currTcb->t_waitingId = mutex->mutex_id;
			addToEndOfLinkedList(currTcb,blockedList);
			tot_cntx_switches++;
			swapcontext(currTcb->t_context,scheduler_ctx);
			pauseTimer();
		}

		mutex->mutex_status = LOCKED;
		mutex->holding_thread = currTcb;
		
		resumeTimer();
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
	//Pause Timer
	pauseTimer();
	printf("Mutex Unlock: Thread: %d\n", currTcb->t_Id);

	if(currTcb->t_Id == mutex->holding_thread->t_Id){
		mutex->mutex_status = UNLOCKED;
		mutex->holding_thread = NULL;
		alertMutexThreads(mutex->mutex_id);
	}
	//printf("about to leave unlock\n");
	
	
	resumeTimer();

	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	pauseTimer();
	// - de-allocate dynamic memory created in worker_mutex_init
	//printf("we in destroy\n");
	t_mutexNode* curr = mutex_list;
	//t_mutexNode* prev = mutex_list;
	if(curr->data->mutex_id == mutex->mutex_id){
		mutex_list = curr->next;
		//doo the free stuff
		free(curr);
		//resumeTimer();
		return 0;
	}
	while(curr->next != NULL){
		if(curr->next->data->mutex_id == mutex->mutex_id){
			t_mutexNode* temp = curr->next;
			curr->next = curr->next->next;
			//free the stuff
			free(temp);
			break;
		}
		else{
			curr = curr->next;
		}
	}
	
	resumeTimer();
	return 0;
};

void printLL(t_node* list) {
	t_node* temp = list;
	while(temp != NULL) {
		//printf("Threads List: %d\n", temp->data->t_Id);
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

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// YOUR CODE HERE
	// printf("Entered scheduler context\n");
	// while(readyQueue->top != NULL){
	// 	printQueue();
	// 	t_node* dequeuedThread = dequeue(readyQueue);
	// 	printf("Scheduler dequeued %d\n", dequeuedThread->data->t_Id);

	// 	currTcb = dequeuedThread->data;
	// 	printf("Swapping to thread context %d\n", dequeuedThread->data->t_Id);
	// 	tot_cntx_switches++;
	// 	swapcontext(scheduler_ctx, dequeuedThread->data->t_context);
		
	// }
	// printf("Exiting scheduler context, queue is empty\n");
	
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
	//printf("Entered scheduler psjf context\n");
    while(1) {
        //printQueue(readyQueue);
		// if(currTcb->timeSinceQuantum >= QUANTUM){
		// 	currTcb->t_quantums++;
		// }
	
		if(currTcb->t_status == READY){
			addToReadyQueue(currTcb,readyQueue);
		}
        t_node* dequeuedThread = dequeue(readyQueue);
        //printf("Scheduler dequeued %d\n", dequeuedThread->data->t_Id);
		if(dequeuedThread == NULL) {
			//printf("Dequeued NULL, scheduler is empty\n");
			dequeuedThread->data = currTcb;
		}

		//Set response time
		// if(dequeuedThread->data->responseTime == 0) {
		// 	struct timespec currentTime;
		// 	clock_gettime(CLOCK_REALTIME, &(currentTime));
		// 	dequeuedThread->data->responseTime = getMicroseconds(currentTime) - dequeuedThread->data->arrivalTime;
		// }
		if(dequeuedThread->data->responseTime.tv_nsec == 0 && dequeuedThread->data->responseTime.tv_sec == 0) {
			struct timespec currentTime;
			clock_gettime(CLOCK_REALTIME, &(currentTime));
			dequeuedThread->data->responseTime = diff_timespec(currentTime, dequeuedThread->data->arrivalTime);
		}

        currTcb = dequeuedThread->data;
        //printf("Swapping to thread context %d\n", dequeuedThread->data->t_Id);
		tot_cntx_switches++;
		resumeTimer();
        swapcontext(scheduler_ctx, dequeuedThread->data->t_context);
    }
    //printf("Exiting scheduler context, queue is empty\n");
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	
	while(1) {
		//printQueue();
		//printf("IN MLFQ SCHED\n");
		//First check the quantum of the current thread (the one that was running before scheduler
		//was called) and update it's priority accordingly
		if(currTcb->t_status == READY) {
			insertToMLFQ(currTcb);
		}

		if(priorityBoostTime >= PRIORITY_BOOST_TIME) {
			priorityBoost();
			priorityBoostTime = 0;
		}

		tcb* nextThreadToRun = dequeueMLFQ();
		if(nextThreadToRun == NULL) {
			//printf("Dequeued NULL, scheduler is empty\n");
			nextThreadToRun = currTcb;
		}
		currTcb = nextThreadToRun;

		//printf("Dequeued in scheduler thread: %d\n", nextThreadToRun->t_Id);
		if(nextThreadToRun->responseTime.tv_nsec == 0 && nextThreadToRun->responseTime.tv_sec == 0) {
			struct timespec currentTime;
			clock_gettime(CLOCK_REALTIME, &(currentTime));
			//nextThreadToRun->responseTime = getMicroseconds(currentTime) - nextThreadToRun->arrivalTime;
		}
		tot_cntx_switches++;
		//resumeTimer();
		swapcontext(scheduler_ctx, nextThreadToRun->t_context);

		// printf("Dequeued in scheduler thread: %d\n", nextThreadToRun->t_Id);
		// currTcb = nextThreadToRun;
		// if(nextThreadToRun->responseTime.tv_nsec == 0 && nextThreadToRun->responseTime.tv_sec == 0) {
		// 	clock_gettime(CLOCK_REALTIME, &(nextThreadToRun->responseTime));
		// 	nextThreadToRun->responseTime = getTimeDiff(nextThreadToRun->responseTime, nextThreadToRun->arrivalTime);
		// }
		// tot_cntx_switches++;
		// resumeTimer();
		// swapcontext(scheduler_ctx, nextThreadToRun->t_context);
		
	}

}

long getMicroseconds(struct timespec timeSpec) {
	return timeSpec.tv_sec*1000000 + timeSpec.tv_nsec/1000;
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
	//Pause Timer
	printf("TIMER RING, in swap_to_scheduler\n");
	pauseTimer();
	currTcb->t_priority == 0 ? currTcb->t_priority = 0 : currTcb->t_priority--;
	currTcb->t_quantums++;
	currTcb->timeSinceQuantum.tv_nsec = 0;
	currTcb->timeSinceQuantum.tv_sec = 0;
	currTcb->lastStart.tv_nsec = 0;
	currTcb->lastStart.tv_sec = 0;
	currTcb->lastEnd.tv_nsec = 0;
	currTcb->lastEnd.tv_sec = 0;

	//Update runtime of thread
	// clock_gettime(CLOCK_REALTIME, &(currTcb->lastEnd));
	// struct timespec lastRunTime = getTimeDiff(currTcb->lastEnd, currTcb->lastStart);
	// currTcb->timeSinceQuantum.tv_nsec += lastRunTime.tv_nsec;
	// currTcb->timeSinceQuantum.tv_sec += lastRunTime.tv_sec;

	//Update runtime of thread (WITH LONGS);
	// struct timespec currentTime;
	// clock_gettime(CLOCK_REALTIME, &(currentTime));
	// currTcb->lastEnd = getMicroseconds(currentTime);
	// long lastRunTime = currTcb->lastEnd - currTcb->lastStart;
	// currTcb->timeSinceQuantum += lastRunTime;

	//currTcb->t_quantums++;
	//enqueue(currTcb, readyQueue);
	tot_cntx_switches++;
	swapcontext(currTcb->t_context, scheduler_ctx);
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

void printQueue(t_queue* queue) {
	//printf("Printing Queue Size: %d\n", queue->size);
	t_node* temp = queue->top;
	if(queue->top == NULL) {
		return;
	}
	while(temp != NULL) {
		printf("Printing Queue: %d\n", temp->data->t_Id);
		temp = temp->next;
	}
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
				//insert(temp->data, readyQueue);
				insertToMLFQ(temp->data);
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
				insertToMLFQ(temp->data);
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

void pauseTimer() {
	// timer.it_value.tv_sec = 0;
	// timer.it_value.tv_usec = 0;
	setitimer(ITIMER_PROF, &zero_timer, &timer);
	updateThreadRuntime(currTcb);
	// struct timespec currentTime;
	// clock_gettime(CLOCK_REALTIME, &currentTime);
	// priorityBoostTime += getMicroseconds(currentTime) -  getMicroseconds(timerLastStarted);
}

void resumeTimer() {
	if(currTcb != NULL) {
		long remainingTime = QUANTUM * 1000 - currTcb->timeSinceQuantum.tv_nsec;
		printf("Remaining time: %ld, for current thread: %d\n", remainingTime, currTcb->t_Id);
		if(remainingTime < 5000) {
			sleep(1);
		}
		if(remainingTime > 1000) {
			//printf("Quantum donezo\n");
			timer.it_value.tv_usec = (int) (remainingTime/1000);
			printf("Timer at: %ld\n", timer.it_value.tv_usec);
		}
		printf("Timer at (outside): %ld\n", timer.it_value.tv_usec);
		clock_gettime(CLOCK_REALTIME, &(currTcb->lastStart));
	}
	//clock_gettime(CLOCK_REALTIME, &timerLastStarted);
	setitimer(ITIMER_PROF, &timer, NULL);
}

void insertToMLFQ(tcb* thread) {
	// int currLevel = MLFQ_QUEUES_NUM - 1;
	// //Check if this thread used it's time quantum
	// if(thread->timeSinceQuantum >= QUANTUM) {
	// 	//printf("insertToMLFQ: in quantum finished\n");
	// 	//If it has, insert it into the next lower priority queue
	// 	//and reset it's timeSinceQuantum, lastStart, and lastEnd
	// 	thread->t_priority == 0 ? enqueue(thread, mlfq[0]) : enqueue(thread, mlfq[thread->t_priority - 1]);
	// 	thread->t_priority == 0 ? thread->t_priority = 0 : thread->t_priority--;
	// 	thread->t_quantums++;
	// 	thread->timeSinceQuantum = 0;
	// 	thread->lastStart = 0;
	// 	thread->lastEnd = 0;
	// }
	// //If it hasn't, insert it to the end of the same priority queue
	// else {
	// 	//printf("insertToMLFQ: in quantum NOT finished. Inserting thread: %d, priority: %d\n", thread->t_Id, thread->t_priority);
	// 	enqueue(thread, mlfq[thread->t_priority]);
	// 	//printf("Printing Queue After Insert MLFQ Inserted\n");
	// 	printQueue(mlfq[2]);

	// }
}

tcb* dequeueMLFQ() {
	int currLevel = MLFQ_QUEUES_NUM - 1;

	for(currLevel; currLevel >= 0; currLevel--) {
		if(mlfq[currLevel]->size != 0) {
			return dequeue(mlfq[currLevel])->data;
		}
		//t_node* curr = mlfq[currLevel]->top;
		// while(curr != NULL) {
		// 	if(curr->data->t_status == READY) {
		// 		return curr->data;
		// 	}
		// 	curr = curr->next;
		// }
	}
	return NULL;
}

void priorityBoost() {
	int currLevel = MLFQ_QUEUES_NUM - 2;

	for(currLevel; currLevel >= 0; currLevel--) {
		while(mlfq[currLevel]->size != 0) {
			t_node* curr = dequeue(mlfq[currLevel]);
			curr->data->t_priority = MLFQ_QUEUES_NUM - 1;
			enqueue(curr->data, mlfq[MLFQ_QUEUES_NUM - 1]);
		}
	}
}

void updateThreadRuntime(tcb* thread) {
	// if(tcb != NULL) {
	// 	struct timespec currentTime;
	// 	clock_gettime(CLOCK_REALTIME, &(currentTime));
	// 	tcb->lastEnd = getMicroseconds(currentTime);
	// 	long lastRunTime = tcb->lastEnd - tcb->lastStart;
	// 	tcb->timeSinceQuantum += lastRunTime;

	// 	if(tcb->timeSinceQuantum >= QUANTUM) {
	// 		//printf("Time since q hit\n");
	// 		tcb->t_priority == 0 ? tcb->t_priority = 0 : tcb->t_priority--;
	// 		tcb->t_quantums++;
	// 		tcb->timeSinceQuantum = 0;
	// 		tcb->lastStart = 0;
	// 		tcb->lastEnd = 0;
	// 		//priorityBoostCounter++;
	// 	}
	// 	// else {
	// 	// 	struct timespec currentTime;
	// 	// 	clock_gettime(CLOCK_REALTIME, &(currentTime));
	// 	// 	tcb->lastEnd = getMicroseconds(currentTime);
	// 	// 	long lastRunTime = tcb->lastEnd - tcb->lastStart;
	// 	// 	tcb->timeSinceQuantum += lastRunTime;
	// 	// }
	// }
	if(thread != NULL) {
		clock_gettime(CLOCK_REALTIME, &(thread->lastEnd));
		struct timespec lastRunTime = diff_timespec(thread->lastEnd, thread->lastStart);
		thread->timeSinceQuantum = add_timespec(thread->timeSinceQuantum, lastRunTime);
		printf("Time since quantum ns: %ld\n", thread->timeSinceQuantum.tv_nsec);
		// long remainingTime = QUANTUM * 1000 - currTcb->timeSinceQuantum.tv_nsec;

		// if(remainingTime <= 1000) {
		// 	//printf("Time since q hit\n");
		// 	tcb->t_priority == 0 ? tcb->t_priority = 0 : tcb->t_priority--;
		// 	tcb->t_quantums++;
		// 	tcb->timeSinceQuantum.tv_nsec = 0;
		// 	tcb->timeSinceQuantum.tv_sec = 0;
		// 	tcb->lastStart.tv_nsec = 0;
		// 	tcb->lastStart.tv_sec = 0;
		// 	tcb->lastEnd.tv_nsec = 0;
		// 	tcb->lastEnd.tv_sec = 0;
		// 	//priorityBoostCounter++;
		// }
		// else {
		// 	struct timespec currentTime;
		// 	clock_gettime(CLOCK_REALTIME, &(currentTime));
		// 	tcb->lastEnd = getMicroseconds(currentTime);
		// 	long lastRunTime = tcb->lastEnd - tcb->lastStart;
		// 	tcb->timeSinceQuantum += lastRunTime;
		// }
	}
	
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

struct timespec add_timespec(struct timespec time1, struct timespec time2) {
	struct timespec result;
	
	long nsec = time1.tv_nsec + time2.tv_nsec;
	long sec = time1.tv_sec + time2.tv_sec;

	while(nsec >= 1000000000) {
		sec += 1;
		nsec -= 1000000000;
	}

	result.tv_nsec = nsec;
	result.tv_sec = sec;
	
	return result;
}