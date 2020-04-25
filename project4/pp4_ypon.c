#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    pid_t pid;

    char parent_message[100];   // sent to child
    char parent_response[100];  // received by child
    char child_message[100];    // sent to parent
    char child_response[100];   // received by parent

    int parent_to_child_pipe[2];
    int child_to_parent_pipe[2];

    if ((pipe(parent_to_child_pipe)) == -1) {
        fprintf(stderr, "Pipe 1 creation error -- errno: %d.\n", errno);
    }
    if ((pipe(child_to_parent_pipe)) == -1) {
        fprintf(stderr, "Pipe 2 creation error -- errno: %d.\n", errno);
    }

    pid = fork();

    if (pid < 0) {
        /* Error */
        fprintf(stderr, "Fork error -- errno: %d.\n", errno);

    } else if (pid == 0) {
        /* child process */

        // receiving message from parent
        if((close(parent_to_child_pipe[1])) == -1) {
            fprintf(stderr, "Error closing write (child) -- errno: %d.\n", errno);
        }
        if((read(parent_to_child_pipe[0], parent_response, 100)) == -1) {
            fprintf(stderr, "Reading from parent failed -- errno: %d.\n", errno);
        }
        printf("From parent: %s", parent_response);

        // sending message to parent
        sprintf(child_message, "Daddy, my name is %d.\n", getpid());
        if((close(child_to_parent_pipe[0])) == -1) {
            fprintf(stderr, "Error closing read (child) -- errno: %d.\n", errno);
        }
        if((write(child_to_parent_pipe[1], child_message, sizeof(child_message)+1)) == -1) {
            fprintf(stderr, "Writing to parent failed -- errno: %d.\n", errno);
        }

    } else if (pid > 0) {
        /* parent process */

        // sending message to child
        sprintf(parent_message, "I am your daddy! and my name is %d.\n", getpid());
        if((close(parent_to_child_pipe[0])) == -1) {
            fprintf(stderr, "Error closing read (parent) -- errno: %d.\n", errno);
        }
        if((write(parent_to_child_pipe[1], parent_message, sizeof(parent_message)+1)) == -1) {
            fprintf(stderr, "Writing to child failed -- errno: %d.\n", errno);
        }

        // waiting on child
        int exit_status;
        if(wait(&exit_status) < 0){
            fprintf(stderr, "Error waiting for child -- errno: %d.\n", errno);
        }
        if(!WIFEXITED(exit_status) || WEXITSTATUS(exit_status) != 0){
            fprintf(stderr, "Child exitted abnormally -- errno: %d.\n", errno);
        }

        // receiving message from child
        if((close(child_to_parent_pipe[1])) == -1) {
            fprintf(stderr, "Error closing write (parent) -- errno: %d.\n", errno);
        }
        if((read(child_to_parent_pipe[0], child_response, 100)) == -1) {
            fprintf(stderr, "Reading from child failed -- errno: %d.\n", errno);
        }
        printf("From child: %s", child_response);

        exit(0);
    }

    return 0;
}
