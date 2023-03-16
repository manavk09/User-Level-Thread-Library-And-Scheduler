#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../thread-worker.h"

/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

int foo(){
	printf("foo\n");
	int i = 50;
	pthread_exit(&i);
}

int bar(){
	printf("bar\n");
	worker_yield();
	int i = 60;
	pthread_exit(&i);
}

int main(int argc, char **argv) {

	/* Implement HERE */
	pthread_t fooThread;
	pthread_t barThread;
	int* fooValue = malloc(sizeof(int));
	int* barValue = malloc(sizeof(int));
	
	pthread_create(&barThread, NULL, &bar, 0);
	pthread_create(&fooThread, NULL, &foo, 0);
	
	//printf("In main, going to join.\n");

	pthread_join(fooThread, (void *) &fooValue);
	pthread_join(barThread, (void *) &barValue);
	printf("Foo value: %d, Bar value: %d\n", *fooValue, *barValue);

	return 0;
}