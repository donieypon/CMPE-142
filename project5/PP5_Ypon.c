#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

// thread conditional variable
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; // signaled by thread 1
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER; // signaled by thread 2

// mutex
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int turn = 0;

// when set to 1, will exit the program
int ex = 0;

// do not exit from handler!!
void SIGINT_handler(int sig) {
    ex = 1;
}

void *pingPong(void *arg) {

    do {
        // acquire lock
        if((pthread_mutex_lock(&lock)) != 0) {
	         fprintf(stderr, "mutex lock error -- errno: %d.\n", errno);
	        }

        if (turn == 0) {
            /* thread 1 */

            printf("thread 1: ping thread 2\n");
            turn = 1;

            // signal thread 2
            if ((pthread_cond_signal(&cond2)) != 0) {
                fprintf(stderr, "cond2 wait error -- errno: %d.\n", errno);
            }

            // blocking
            if ((pthread_cond_wait(&cond1, &lock)) != 0) {
                fprintf(stderr, "cond1 signal error -- errno: %d.\n", errno);
            }

            // prints when signaled by thread 2
            printf("thread 1: pong! thread 2 ping received\n");
        } else {
            /* thread 2 */

            // prints when thread 2 is unblocked
            printf("thread 2: pong! thread 1 ping received\n");
            turn = 0;
            printf("thread 2: ping thread 1\n");

            // signals thread 1
            if ((pthread_cond_signal(&cond1)) != 0) {
                fprintf(stderr, "cond1 signal error -- errno: %d.\n", errno);
            }
        }

        if ((pthread_mutex_unlock(&lock)) != 0) {
	         fprintf(stderr, "mutex unlock error -- errno: %d.\n", errno);
	      }

        // see ping pong interactions
        sleep(2);

    } while(ex == 0);

    exit(0);
}

int main() {
    pthread_t thread1;
    pthread_t thread2;

    // initialize signals w/ error handling
    if ((signal(SIGINT, SIGINT_handler)) == SIG_ERR) {
		    perror("SIGINT signal error.\n");
	  }

    // create threads
    if ((pthread_create(&thread1, NULL, &pingPong, NULL)) != 0) {
        fprintf(stderr, "thread 1 create error -- errno: %d.\n", errno);
    }
    if ((pthread_create(&thread2, NULL, &pingPong, NULL)) != 0) {
        fprintf(stderr, "thread 2 create error -- errno: %d.\n", errno);
    }

    // wait for threads to terminate
    if ((pthread_join(thread1, NULL)) != 0) {
        fprintf(stderr, "thread 1 join error -- errno: %d.\n", errno);
    }
    if ((pthread_join(thread2, NULL)) != 0) {
        fprintf(stderr, "thread 2 join error -- errno: %d.\n", errno);
    }

    return 0;
}
