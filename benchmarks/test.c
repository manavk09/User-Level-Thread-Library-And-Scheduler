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

pthread_mutex_t mutex;

int foo() {
	printf("foo\n");
	int i = 50;
	pthread_mutex_lock(&mutex);
	printf("foo locked\n");
	worker_yield();
	pthread_mutex_unlock(&mutex);
	printf("foo unlocked\n");
	// while(1) {
	// 	//printf("fooing\n");
	// }
	pthread_exit(&i);
}

int bar(){
	printf("bar\n");
	int i = 60;
	//sleep(5);
	//printf("bar woke up\n");
	pthread_mutex_lock(&mutex);
	printf("bar locked\n");
	pthread_mutex_unlock(&mutex);
	printf("bar unlocked\n");
	// while(1) {
	// 	//printf("barring\n");
	// }
	pthread_exit(&i);
}

int main(int argc, char **argv) {

	/* Implement HERE */
	pthread_t fooThread;
	pthread_t barThread;
	int* fooValue = malloc(sizeof(int));
	int* barValue = malloc(sizeof(int));
	pthread_mutex_init(&mutex, NULL);


	pthread_create(&barThread, NULL, &bar, 0);
	pthread_create(&fooThread, NULL, &foo, 0);

	
	//printf("In main, going to join.\n");
	pthread_join(barThread, (void *) &barValue);
	pthread_join(fooThread, (void *) &fooValue);
	//printf("Foo join done in main\n");

	//printf("Foo value: %d, Bar value: %d\n", *fooValue, *barValue);
	print_app_stats();

	return 0;
}