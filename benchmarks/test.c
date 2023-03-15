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
}
int main(int argc, char **argv) {

	/* Implement HERE */
	pthread_t thread;
	pthread_create(&thread,NULL,&foo,0);

	return 0;
}
