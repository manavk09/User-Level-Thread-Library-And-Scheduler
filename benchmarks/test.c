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

void foo(){
	printf("foo\n");
	pthread_exit(NULL);
}

void bar(){
	printf("bar\n");
	pthread_exit(NULL);
}

int main(int argc, char **argv) {

	/* Implement HERE */
	pthread_t fooThread;
	pthread_t barThread;
	pthread_create(&fooThread, NULL, &foo, 0);
	pthread_create(&barThread, NULL, &bar, 0);

	printf("In main, going to join.\n");
	pthread_join(fooThread, NULL);
	pthread_join(barThread, NULL);

	return 0;
}
