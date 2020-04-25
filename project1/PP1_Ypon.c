#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

//argc = number of input parameters
//argv = array of inputs

/*
   build: gcc PP1_Ypon.c -o zombifier
   run: ./zombifier -n [# of zombies]
*/

/*
   Comments about code:
   Unfortunately, I could not get the signal handler to clean up the zombies. 
   I believe the error lies with the child exiting the process and have implemented
   some error checking. However, that block of code seems to be ommitted when the
   program is ran. 

   Alternatively, if I remove the exit(0) call altogether, calling kill -s SIGCONT [zombie 
   pid] successfully kills one zombie process. After the first signal handling, it does
   not run. 
*/

void sigcont_handler(int sig);

int main(int argc, char **argv[]) {
	
	//varibles for exit() error checking
	int status;
	int errnum;
	
	//create n number of zombies
	int zombies = atoi(argv[2]);
	
	//initializing signal handler
	signal(SIGCONT, sigcont_handler);	

	for(int i=0; i<zombies; i++) {

		int pid = fork();

		if(pid > 0) {
			//inside the parent process
			//make parents sleep for 3 seconds

			printf("parent_pid = %d\n", getpid());
			sleep(10);
		} else if(pid == 0) {
			//inside the child process
			//make child exit before parent

			printf("zombie #%d created.\n", i);
			printf("zombie_pid = %d\n", getpid());
			printf("---------------------\n");
			exit(0);
			
			//error checking: exit()
			int exit_status = WEXITSTATUS(status);
			
			if(exit_status > 0) {
				errnum = errno;
				fprintf(stderr, "Value of errno: %d\n", errno);
				perror("Error printed by perror");
				fprintf(stderr, "Error exiting: %s\n", sterror(errnum));
			} else if(exit_status == 0) {
				printf("Child exited successfully.\n");
			}

		} else {
			//error checking: fork()
			fprintf(stderr, "%s", "fork() error\n");
		}
	}

	return 0;
}

void sigcont_handler(int sig) {
	printf("killed zombie pid:#%d with parent pid:#%d.\n", getpid(), getppid());

}
