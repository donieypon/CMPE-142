#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/memfd.h>

#define SOCK_PATH "mysocket"

// create shmem file desrcriptor
static inline int memfd_create(const char *name, unsigned int flags) {
  return syscall(__NR_memfd_create, name, flags);
}

// receiving fd
static int receive_fd(int conn) {
    struct msghdr msgh;
    struct iovec iov;

    union {
        struct cmsghdr cmsgh;
        char   control[CMSG_SPACE(sizeof(int))];
    } control_un;

    struct cmsghdr *cmsgh;

    char placeholder;
    iov.iov_base = &placeholder;
    iov.iov_len = sizeof(char);

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    int size = recvmsg(conn, &msgh, 0);
    if (size == -1)
        printf("recvmsg()\n");

    if (size != 1) {
        printf("Expected a placeholder message data of length 1");
        printf("Received a message of length %d instead\n", size);
    }

    cmsgh = CMSG_FIRSTHDR(&msgh);
    if (!cmsgh)
        printf("Expected one recvmsg() header with a passed memfd fd. "
       "Got zero headers!");

    if (cmsgh->cmsg_level != SOL_SOCKET)
        printf("invalid cmsg_level %d", cmsgh->cmsg_level);
    if (cmsgh->cmsg_type != SCM_RIGHTS)
        printf("invalid cmsg_type %d", cmsgh->cmsg_type);

    return *((int *) CMSG_DATA(cmsgh));
}

// make fd usable
char *shmc;
static int getmemfd(){

    /*shared memory variables*/
    const int shm_sz = 1024;
    int fd, ret;

    fd = memfd_create("Server_memfd",MFD_ALLOW_SEALING);
    if (fd == -1)
        perror("memfd_create()");

    ret = ftruncate(fd, shm_sz);
    if (ret == -1)
        perror("ftruncate()");


    shmc = mmap(NULL, shm_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmc == MAP_FAILED)
        perror("mmap()");

    return fd;
}

// send shmem fd
static void send_fd(int conn, int fd) {
    struct msghdr msgh;
    struct iovec iov;

    union {
        struct cmsghdr cmsgh;
        char   control[CMSG_SPACE(sizeof(int))];
    } control_un;

    if (fd == -1) {
        fprintf(stderr, "Cannot pass an invalid fd equaling -1\n");
        exit(EXIT_FAILURE);
    }

    char placeholder = 'A';
    iov.iov_base = &placeholder;
    iov.iov_len = sizeof(char);

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    control_un.cmsgh.cmsg_len = CMSG_LEN(sizeof(int));
    control_un.cmsgh.cmsg_level = SOL_SOCKET;
    control_un.cmsgh.cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh))) = fd;

    int size = sendmsg(conn, &msgh, 0);
    if (size < 0)
        perror("sendmsg()");
}

// used to control user input
int stop = 0;

int main() {

  // socket variables
  struct sockaddr_un server, client;
  int sockfd;
  int len;

  // parent -> server, child -> client
  int pid = fork();

  // semaphores for synchronization
  sem_t *sem1, *sem2;

    // create, initialize semaphores
  sem1 = sem_open("/sema1", O_CREAT,  0644, 0);
  sem2 = sem_open("/sema2", O_CREAT,  0644, 1);

  if(pid < 0) {
    // fork error
    printf("Fork failed\n");
    exit(1);
  } else if(pid > 0) {
    /* parent/server start */

    int fd;
    if ((fd = getmemfd()) == -1) {
      printf("fd error.\n");
    }

    // create socket
    if ((sockfd  = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "Producer socket error -- errno: %d.\n", errno);
      exit(1);
    }

    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, SOCK_PATH);
    unlink(server.sun_path); //to remove link if it already exists
    len = strlen(server.sun_path) + sizeof(server.sun_family);

    // binding
    if((bind(sockfd, (struct sockaddr*)&server, len)) == -1){
      fprintf(stderr, "Parent bind error -- errno: %d.\n", errno);
      exit(1);
    }

    // listen to incoming connections
    if(listen(sockfd, 1) == -1){
      fprintf(stderr, "Parent listen failed -- errno: %d.\n", errno);
    }

    // accept connection
    int s2;
    len = sizeof(client) ;
    if((s2 = accept(sockfd, (struct sockaddr *)&client, &len))== -1){
        fprintf(stderr, "Producer accept error -- errno: %d.\n", errno);
        exit(1);
    }

    // send fd
    send_fd(s2, fd);
    char msg[1024];

    // continuosly receive user input
    do {
      sem_wait(sem2);

      printf("Enter message or type 'exit' to stop:\n");
      scanf("%s", msg);
      sprintf(shmc,msg);
      if(strcmp(shmc, "exit") == 0) {
        stop = 1;
      }

      sem_post(sem1);
    } while(stop == 0);

    int exit_status;
    if(wait(&exit_status) < 0){
        perror("Error waiting for child.\n");
    }
    if(!WIFEXITED(exit_status) || WEXITSTATUS(exit_status) != 0){
        perror("Child exitted abnormally.\n");
    }
    close(s2);
    unlink("s");

    close(s2);
    close(fd);
    sem_close(sem1);
    sem_unlink("/sema1");
    sem_close(sem2);
    sem_unlink("/sema2");

    printf("Parent closed socket connection, fd, and semaphores.\n");

    exit(0);
    /* parent/server END */
  } else {
    /* child/client START */

    char *shm, *shm1, *shm2;
    const int shm_size = 1024;
    int ret, fdc, seals;

    sleep(2);

    int sockfd_c;
    if ((sockfd_c = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Child create socket failed -- errno: %d.\n", errno);
    }

    client.sun_family = AF_UNIX;
    strcpy(client.sun_path, SOCK_PATH);
    len = strlen(client.sun_path) + sizeof(client.sun_family);

    // connet
    if (connect(sockfd_c, (struct sockaddr *)&client, len) == -1) {
         fprintf(stderr, "Error child connect -- errno: %d.\n", errno);
          exit(1);
    }

    if ((fdc = receive_fd(sockfd_c)) == -1)
        printf("Received invalid memfd fd from server equaling -1.\n");


    if ((shm = mmap(NULL, shm_size, PROT_READ, MAP_PRIVATE, fdc, 0)) == MAP_FAILED)
        printf("mmap error.\n");

    // receive inputs from shared memory
    do {
      sem_wait(sem1);

      if(strcmp(shm, "exit") == 0) {
        stop = 1;
      } else {
	      printf("Read from shared memory: %s\n", shm);
      }

      sem_post(sem2);
    } while(stop == 0);

    exit(0);
    /* child/client END */
  }

  return 0;
}
