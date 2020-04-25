#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mman.h>

/* 
   To run: (-u for socket, -s for shared memory)
   ./producer-consumer -p -u -q 3 -m hello -e
   ./producer-consumer -c -u -q 3 -e 
 */

#define SOCK_PATH "echo_socket"
#define SHM_SIZE 1024

int buffer_len = 0;

sem_t* buffer_mutex; // acts as mutex lock
sem_t* empty;
sem_t* full;

// exit the loop
int ex = 0;
void SIGINT_handler(int sig) {
  ex = 1;
  printf("SIGINT received\n");
}

int main(int argc, char** argv) {

  // signal handler
  if ((signal(SIGINT, SIGINT_handler)) == SIG_ERR) {
    fprintf(stderr, "Error initializing signal -- errno: %d.\n", errno);
  }

  int p_c = -1; // can be producer or client
  int sock_shmem = -1; // socket or shared memory
  int e = 0; // print statements
  char message[1024]; // message for producer

  // socket variables
  struct sockaddr_un local, remote; // local -> server, remote -> client
  int socketfd;
  int len;

  // shared memory variables
  key_t key;
  int shmid; // shared mem id
  char *data;
  int mode;

  // shared semaphores
  const char *sem1 = "full";
  const char *sem2 = "empty";
  const char *sem3 = "buffer_mutex";
  int shmemfd;
  int *shelf;
  const char *n = "shared_memory";
  shmemfd = shm_open(n, O_CREAT | O_RDWR, 0666);
  ftruncate(shmemfd, sizeof(int));
  shelf = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmemfd, 0);

  // retreive input from command line
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i], "-p")) { // producer
      p_c = 1;
    } else if (!strcmp(argv[i], "-c")) { // consumer
      p_c = 0;
    } else if (!strcmp(argv[i], "-m")) { // message
      strcpy(message, argv[i+1]);
    } else if (!strcmp(argv[i], "-q")) { // buffer size
      buffer_len = atoi(argv[i+1]);
    } else if (!strcmp(argv[i], "-u")) { // socket or shared memory
      sock_shmem = 1;
    } else if (!strcmp(argv[i], "-e")) { // print statements
      e = 1;
    }
  }

  // check for errros from command line
  if (p_c == -1) {
    printf("Error: did not indicate producer or consumer.\n");
    exit(0);
  }
  if (strcmp(message, "") == 0) {
    printf("Error: invalid message.\n");
    exit(0);
  }
  if (buffer_len == -1) {
    printf("Error: did not indicate queue size.\n");
    exit(0);
  }
  if (sock_shmem == -1) {
    printf("Error: did not indicate to use socket or shared memory.\n");
    exit(0);
  }

  // open semaphores and initialize values
  full = sem_open(sem1, O_CREAT, 0666, 0);
  empty = sem_open(sem2, O_CREAT, 0666, buffer_len);
  buffer_mutex = sem_open(sem3, O_CREAT, 0666, 1);

  if (p_c == 1) {
    /* producer START */

    if (sock_shmem) {
      /* producer with sockets START */
      printf("Producer with sockets\n");

      // create socket
      socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (socketfd == -1) {
        fprintf(stderr, "Error producer socket -- errno: %d.\n", errno);
      }

      //printf("Producer socket created.\n");

      local.sun_family = AF_UNIX;
      strcpy(local.sun_path, SOCK_PATH);
      unlink(local.sun_path);
      len = strlen(local.sun_path) + sizeof(local.sun_family);

      // binding producer to socket
      if (bind(socketfd, (struct sockaddr *)&local, len) == -1) {
        fprintf(stderr, "Error producer bind -- errno: %d.\n", errno);
      }

      //printf("Producer socket binded\n");

      // listen for connections
      if (listen(socketfd, 1) == -1) {
        fprintf(stderr, "Error producer listen -- errno: %d.\n", errno);
      }

      // accept connection
      int t = sizeof(remote);
      int sock_connect = accept(socketfd, (struct sockaddr *)&remote, &t);
      if (sock_connect == -1) {
        fprintf(stderr, "Error producer accept -- errno: %d\n", errno);
      }

      //printf("Producer socket accepted\n");

      do {
        int val;
        sem_getvalue(empty, &val);
        printf("empty val: %d\n", val);

        sem_wait(empty);
        sem_wait(buffer_mutex);

        if (write(sock_connect, message, sizeof(message)) == -1) {
          fprintf(stderr, "error producer write -- errno: %d\n", errno);
        }

        if (e) {
          printf("produced message: %s.\n", message);
        }

        sem_post(buffer_mutex);
        sem_post(full);

        sleep(2);
      } while(ex == 0);

      if (close(socketfd) == -1) {
        fprintf(stderr, "producer socket close error -- errno: %d\n", errno);
      }

      /* producer with sockets END */
    } else {
      /* producer with shared memory START */

      // assign IPC identifier to key
      if ((key = ftok(".", 'x')) == -1) {
        fprintf(stderr, "Producer ftok error -- errno: %d.\n", errno);
      }

      // assign shmem identifier to shmid
      if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
        fprintf(stderr, "Producer shmget error -- errno: %d.\n", errno);
      }

      // mapping shared memory
      data = shmat(shmid, (void *)0, 0);
      if (data == (char *)(-1)) {
        fprintf(stderr, "Producer shmat error -- errno: %d.\n", errno);
      }

      do {
        int val;
        sem_getvalue(empty, &val);
        printf("%d\n",val );

        sem_wait(empty);
        sem_wait(buffer_mutex);

        strncpy(data, message, SHM_SIZE);

        if (e) {
          printf("produced message: %s\n", data);
        }

        sem_post(buffer_mutex);
        sem_post(full);

        sleep(2);
      } while(ex == 0);

      /* producer with shared memory END */
    }

    /* producer END */
  } else {
    /* consumer START */

    if (sock_shmem) {
      /* consumer with sockets START */
      printf("Consumer with sockets\n");

      char producer_message[1024]; // consume messages

      // create socket
      socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (socketfd == -1) {
        fprintf(stderr, "Consumer create socket error -- errno: %d.\n", errno);
      }

      //printf("Consumer socket created\n");

      remote.sun_family = AF_UNIX;
      strcpy(remote.sun_path, SOCK_PATH);
      len = strlen(remote.sun_path) + sizeof(remote.sun_family);

      // connect
      if (connect(socketfd, (struct sockaddr *)&remote, len) == -1) {
        fprintf(stderr, "Consumer socket connecr error -- errno: %d.\n", errno);
      }

      //printf("Consumer socket connected\n");

      do {
        int val;
        sem_getvalue(full, &val);
        printf("full value: %d\n",val );

        sem_wait(full);
        sem_wait(buffer_mutex);

        if (read(socketfd, producer_message, sizeof(producer_message)) == -1) {
          fprintf(stderr, "Consumer socket read error -- errno: %d\n", errno);
        }

        if (e) {
          printf("consumed messsage: %s\n", producer_message);
        }

        sem_post(buffer_mutex);
        sem_post(empty);

        sleep(2);
      } while(ex == 0);

      if (close(socketfd) == -1) {
        fprintf(stderr, "Consumer socket close error -- errno: %d\n", errno);
      }

      /* consumer with sockets END */
    } else {
      /* consumer with shared memory START */

      key = ftok(".", 'x');

      if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
        fprintf(stderr, "Consumer shmget error -- errno: %d.\n", errno);
      }

      // retrive data from producer
      data = shmat(shmid, (void *)0, 0);
      if (data == (char *)(-1)) {
        fprintf(stderr, "Consumer shmat error -- errno: %d.\n", errno);
      }

      do {
        int val;
        sem_getvalue(full, &val);
        printf("full value: %d\n",val );

        sem_wait(full);
        sem_wait(buffer_mutex);

        if (e) {
          printf("consumed message: %s\n", data);
        }

        sem_post(buffer_mutex);
        sem_post(empty);

        sleep(2);
      } while(ex == 0);

      /* consumer with shared memory END */
    }

    /* consumer END */
  }

  // close and unlink semaphores
  printf("Closing and unlinking semaphores...\n");
  sem_close(full);
  sem_close(empty);
  sem_close(buffer_mutex);
  sem_unlink(sem1);
  sem_unlink(sem2);
  sem_unlink(sem3);

  // close shared memory segments
  printf("Closing and unlinking shared memory...\n");
  munmap(shelf, sizeof(int));
  close(shmemfd);
  shm_unlink(n);

  return 0;
}
