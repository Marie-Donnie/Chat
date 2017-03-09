
/*----------------------------------------------
  Client application
  ------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

/*--------- Define struct types to use it more easily ---------*/

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;

/*--------- Define constants ---------*/

#define MAX_NAME_SIZE 256
#define SERVER_PORT 5000


void *read_loop(void *arg){
  int length;
  char buffer[256];
  int socket_descriptor = *(int *)arg;
    /* listen to the server answer */
  while ((length = read(socket_descriptor, buffer, sizeof(buffer))) > 0) {
    write(fileno(stdout),buffer,length);
  }

  return NULL;
}

int main(int argc, char **argv) {
  int socket_descriptor;    /* socket descriptor */
  sockaddr_in local_address;  /* socket local address */
  hostent * ptr_host;   /* informations about host machine */
  char *soft; /* software name */
  char *host;  /* distant host name */
  char msg[256];  /* sent message */
  char name[MAX_NAME_SIZE]; /* user name */
  int msg_size; /* message size */
  pthread_t thread;

  if (argc != 3) {
    perror("usage : client <server-address> <user-name>");
    exit(1);
  }
  soft = argv[0];
  host = argv[1];
  sprintf(name, "/nick %s", argv[2]);
  printf("software name: %s ; server address: %s ; name chosen: %s \n", soft, host, name);

  if ((ptr_host = gethostbyname(host)) == NULL) {
    perror("error: cannot find server");
    exit(1);
  }
  /* character copy of the ptr_host informations to local_address */
  bcopy((char*)ptr_host->h_addr, (char*)&local_address.sin_addr, ptr_host->h_length);
  local_address.sin_family = AF_INET; /* ou ptr_host->h_addrtype; */
  /* use defined port */
  local_address.sin_port = htons(SERVER_PORT);
  /*-----------------------------------------------------------*/
  printf("port number to use for server connection: %d \n", ntohs(local_address.sin_port));
  /* define socket */
  if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("error: unable to create the connection socket.");
    exit(1);
  }
  /* attempt to connect to the server described in local_address */
  if ((connect(socket_descriptor, (sockaddr*)(&local_address), sizeof(local_address))) < 0) {
    perror("error: unable to connect to the server.");
    exit(1);
  }
  printf("Connection established. \n");

  /* Send name to the server */
  write(socket_descriptor, name, strlen(name));

  /* Handle the reception of messages from the server */
  pthread_create(&thread, NULL, read_loop, (void *)&socket_descriptor);

  /* Handle the sending of messages */
  /* read is blocking so the loop is used only when a message is read */
  while ( (msg_size = read(fileno(stdin), msg, sizeof(msg))) > 0){

    /* send message to the server */
    /* printf("Sending message to the server. \n"); */
    if ((write(socket_descriptor, msg, msg_size)) < 0) {
      perror("error: unable to send the message.");
      exit(1);
    }
    /* printf("Message sent to the server. \n"); */

  }

  printf("\nEnd of the transmission.\n");
  close(socket_descriptor);
  printf("Connection to the server closed.\n");


  return EXIT_SUCCESS;
}
