#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

bool sig_SIGINT = false;

void SIGINT_handler(int sig) {
    if (sig_SIGINT) {
        sig_SIGINT = false;
    } else {
      sig_SIGINT = true;
    }
}

void SIGUSR1_handler(int sig) {
		exit(0);
}

int main() {
    // counts # of iterations
    int iterations = 0;

        // initialize signal handers w/ error handling
		if ((signal(SIGINT, SIGINT_handler)) == SIG_ERR) {
				fprintf(stderr, "SIGINT signal error -- errno: %d", errno);
		}
		if ((signal(SIGUSR1, SIGUSR1_handler)) == SIG_ERR) {
				fprintf(stderr, "SIGUSR1 signal error -- errno: %d", errno);
		}

		// use PID to call SIGUSR1
		printf("PID: %d\n", getpid());

    // infinite loop w/ sleep intervals of 2 seconds
    while(1) {
        sleep(2);
        //printf("Slept for 2 seconds.\n");

        // check for signals
        if (sig_SIGINT) {
            printf("# of iterations: %d\n", iterations);
        }

        // increment
        iterations++;
    }

    return 0;
}
