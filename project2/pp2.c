#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>

#define SOCK_PATH "echo_socket"

int main() {
  int s, len;
  struct sockaddr_un local, remote; //local = server, remote = client
  char server_message[100] = "Ping\n"; //sent by parent
  char server_response[100]; //received by child
  char client_message[100] = "Pong\n"; //sent by child
  char client_response[100]; //received by parent

  //make parent the server, child the client
  pid_t pid = fork();

  //check if socket is created
  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket error");
    exit(1);
  }

  printf("Socket created.\n");

  if (pid < 0) {

    perror("Fork failed.");
    exit(1);

  } else if (pid == 0) {
      //child = client
      //make child sleep to let parent connect to socket first
      sleep(10);

      remote.sun_family = AF_UNIX;
      strcpy(remote.sun_path, SOCK_PATH);
      len = strlen(remote.sun_path) + sizeof(remote.sun_family);

      printf("Child connecting to socket...\n");

      //could not connect to socket
      if (connect(s, (struct sockaddr *)&remote, len) == -1) {
          perror("Child connect to socket failed.\n");
          exit(1);
      }

      //child connected to socket
      printf("Child connected to socket.\n");
      printf("Child receiving...\n");

      if ((recv(s, &server_response, sizeof(server_response), 0)) == -1) {
          perror("Child recv failed.\n");
          exit(1);
      }

      printf("Child received from parent: %s\n", server_response);
      printf("Child sending...\n");

      if (send(s, client_message, sizeof(client_message), 0) == -1) {
          perror("Child send failed.\n");
          exit(1);
      }

      printf("Child sent message.\n");

      close(s);

    } else if (pid > 0) {

      //parent = server
      printf("Parent connecting to socket...\n");

      local.sun_family = AF_UNIX;
      strcpy(local.sun_path, SOCK_PATH);
      unlink(local.sun_path);
      len = strlen(local.sun_path) + sizeof(local.sun_family);

      //could not bind
      if (bind(s, (struct sockaddr *)&local, len) == -1) {
          perror("Bind failed.\n");
          exit(1);
      }

      printf("Parent binded to socket.\n");

      //listen for incoming connections
      if (listen(s, 5) == -1) {
          perror("Parent listen failed.\n");
          exit(1);
      }

      printf("Parent listening for connections...\n");

      int s2 = accept(s, (struct sockaddr *)&remote, &len);
      if (s2 == -1) {
          perror("Parent accept failed.\n");
          exit(1);
      }

      printf("Parent accepted.\n");
      printf("Parent sending...\n");

      if ((send(s2, &server_message, sizeof(server_message), 0)) == -1) {
          perror("Parent send failed.\n");
          exit(1);
      }

      printf("Parent sent message.\n");
      printf("Parent receiving...\n");

      if ((recv(s2, &client_response, sizeof(client_response), 0)) == -1) {
          perror("Parent recv failed.\n");
          exit(1);
      }

      printf("Parent received from child: %s", client_response);

      int exit_status;
      if(wait(&exit_status) < 0){
          perror("Error waiting for child.\n");
      }
      if(!WIFEXITED(exit_status) || WEXITSTATUS(exit_status) != 0){
          perror("Child exitted abnormally.\n");
      }
      close(s2);
      unlink("s");
      exit(0);
  }
  return 0;
}
