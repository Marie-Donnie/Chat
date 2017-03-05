
/*----------------------------------------------
  Server-side application
  ------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

/*--------- Define constants and statics ---------*/

#define MAX_NAME_SIZE 256
#define MAX_CLIENT_NUMBER 10
#define SERVER_PORT 5000

static unsigned int clients_number = 0;
static int id = 10;


/*--------- Define struct types to use it more easily ---------*/

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


/* Send message to all clients but the sender */
void send_message(int id){
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
void send_message_to_all(int cli_co){
  char buffer[256];
  int length;
  int i;
  if ((length = read(cli_co, buffer, sizeof(buffer))) <= 0)
    return;
  printf("Income message : %s \n", buffer);
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (clients[i]) {
	write(clients[i]->cli_co, buffer, strlen(buffer)+1);
    }
  }
    printf("Messages sent. \n");
}



main(int argc, char **argv) {
  int socket_descriptor,  /* socket descriptor */
    new_socket_descriptor,  /* new socket descriptor */
    address_length; /* client address length */
  sockaddr_in local_address,    /* local address socket informations */
    cli_addr;  /* client address */
  hostent* ptr_host;  /* informations about host */
  servent* ptr_service;  /* informations about service */
  char host_name[MAX_NAME_SIZE+1];  /* host name */
  int listenfd = 0, cli_co = 0;

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
  /* attente des connexions et traitement des donnees recues */
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
    /* handle the reception of a message */
    printf("Receiving a message.\n");

    /* check if there is already too many clients */
    if ( (clients_number+1) == MAX_CLIENT_NUMBER){
      printf("Too many clients already; client rejected\n");
      close(new_socket_descriptor);
    }

    /* Client settings */
    client *cli = (client *)malloc(sizeof(client));
    cli->addr = cli_addr;
    cli->cli_co = new_socket_descriptor;
    cli->id = id++;
    sprintf(cli->name, "%d", cli->id);
    printf("Client address: %d\n", cli->addr);
    printf("Client id: %d\n", cli->id);

    /* for(int i = 0; i < MAX_CLIENT_NUMBER; i++){ */
    /*   if(!clients[i]){ */
    clients[clients_number] = cli;
    clients_number++;
    /* 	return; */
    /*   } */
    /* } */

    send_message_to_all(cli->cli_co);
    close(new_socket_descriptor);
  }
}
