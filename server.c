
/*----------------------------------------------
  Server-side application
  ------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>


/*--------- Define constants and statics ---------*/

#define MAX_NAME_SIZE 256
#define MAX_CLIENT_NUMBER 10
#define SERVER_PORT 5000

static unsigned int clients_number = 0;
static int id = 10;


/*--------- Define struct types ---------*/

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;
/* Client structure */
typedef struct {
  struct sockaddr_in addr;	/* Client remote address */
  int cli_co;			/* Informations about client*/
  int id;			/* Client identifier */
  char name[MAX_NAME_SIZE];     /* Client name */
} client;


client *clients[MAX_CLIENT_NUMBER];

/*--------- Functions ---------*/

void read_message(char **buffer, int buffer_size, int cli_co){
  int length;
  if ((length = read(cli_co, *buffer, buffer_size)) <= 0){
    //printf("j'ai read \n");
    return;
  }
  (*buffer)[length] = '\0';
  printf("Income message : %s \n", *buffer);
}


/* Send message to all clients but the sender */
void send_message(char *msg, int id){
  char buffer[256];
  int length;
  int i;
  if ((length = read(id, buffer, sizeof(buffer))) <= 0)
    return;
  printf("Income message : %s \n", buffer);
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (clients[i]) {
      if (clients[i]->id != id) {
	write(clients[i]->cli_co, buffer, strlen(buffer)+1);
      }
    }
  }
  printf("Messages sent. \n");
}

/* Send message to all clients */
void send_message_to_all(char *msg){
  int i;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    //printf("s_m_t_a\n");
    if (clients[i]) {
      if ((write(clients[i]->cli_co, msg, strlen(msg)+1)) < 0){
	perror("error : failing to send message to client");
      }
    }
  }
  //printf("Messages sent. \n");
}


void signal_handler(int signal_number){
  printf("Received signal: %s\n", strsignal(signal_number));
}

void add_client(client *cli){
  int i;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (!clients[i]) {
      clients[i] = cli;
      return;
    }
  }
  clients_number++;
}

void remove_client(client *cli){
  int i;
  int cli_id = cli->id;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (clients[i]) {
      if ((clients[i])->id == cli_id) {
	clients[i] = NULL;
	return;
      }
    }
  }
  clients_number--;
}

void *client_loop(void *arg){
  char *buffer = calloc(256, 1);
  int length;
  char join[256];
  char leave[256];

  client *cli = (client *)arg;
  /* handle the reception of a message */
  /* read_message(&buffer, 256, cli->cli_co); */

  sprintf(join, "%d has joined the chat.\n", cli->id);
  send_message_to_all(join);

  while ((length = read(cli->cli_co, buffer, 256)) > 0){
    buffer[length] = '\0';
    printf("Income message : %s \n", buffer);
    send_message_to_all(buffer);
  }
  sprintf(leave, "%d has left the chat", cli->id);
  send_message_to_all(leave);
  close(cli->cli_co);


  remove_client(cli);
  free(cli);
  pthread_detach(pthread_self());
  return NULL;
}

/*--------- Main ---------*/

int main(int argc, char **argv) {
  int socket_descriptor,  /* socket descriptor */
    new_socket_descriptor,  /* new socket descriptor */
    address_length; /* client address length */
  sockaddr_in local_address,    /* local address socket informations */
    cli_addr;  /* client address */
  hostent* ptr_host;  /* informations about host */
  servent* ptr_service;  /* informations about service */
  char host_name[MAX_NAME_SIZE+1];  /* host name */
  int listenfd = 0, cli_co = 0;
  pthread_t thread;

  signal(SIGPIPE, signal_handler);

  gethostname(host_name,MAX_NAME_SIZE);  /* getting host name */

  /* get hostent using server name */
  if ((ptr_host = gethostbyname(host_name)) == NULL) {
    perror("error: unable to find server using its name.");
    exit(1);
  }

  /* initialize sockaddr_in with these informations */
  bcopy((char*)ptr_host->h_addr, (char*)&local_address.sin_addr, ptr_host->h_length);
  local_address.sin_family = ptr_host->h_addrtype;
  /* AF_INET */
  local_address.sin_addr.s_addr = INADDR_ANY;
  /* use the defined port */
  local_address.sin_port = htons(SERVER_PORT);
    /*-----------------------------------------------------------*/
  printf("Using port : %d \n",
	 ntohs(local_address.sin_port) /*ntohs(ptr_service->s_port)*/);
  /* create socket */
  if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("error: unable to create the connection socket.");
    exit(1);
  }
  /* bind socket socket_descriptor to sockaddr_in local_address */
  if ((bind(socket_descriptor, (sockaddr*)(&local_address), sizeof(local_address))) < 0) {
    perror("error: unable to bind the socket to the connection address.");
    exit(1);
  }
  /* initialize the queue */
  listen(socket_descriptor,MAX_CLIENT_NUMBER);


  client *cli;

  for(;;) {
    address_length = sizeof(cli_addr);
    /* cli_addr sera renseignée par accept via les infos du connect */
    if ((new_socket_descriptor =
	 accept(socket_descriptor,
		(sockaddr*)(&cli_addr),
		&address_length))
	< 0) {
      perror("error: unable to accept connection to the client.");
      exit(1);
    }


    /* check if there is already too many clients */
    if ( (clients_number+1) == MAX_CLIENT_NUMBER){
      printf("Too many clients already; client rejected\n");
      close(new_socket_descriptor);
    }

    /* Client settings */
    cli = (client *)malloc(sizeof(client));
    cli->addr = cli_addr;
    cli->cli_co = new_socket_descriptor;
    cli->id = id++;
    sprintf(cli->name, "%d", cli->id);
    printf("Client id: %d\n", cli->id);

    add_client(cli);


    pthread_create(&thread, NULL, client_loop, (void *)cli);


  }


  return EXIT_SUCCESS;
}