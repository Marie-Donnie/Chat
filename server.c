
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


/*--------- Define constants and global variables ---------*/

#define SERVER_PORT 5000         /* Port used for sin_port from sockaddr_in */
#define BUFFER_SIZE 512          /* Size of buffers used */
#define MAX_NAME_SIZE 256        /* Maximum name size for users and channels */
#define MAX_CLIENT_NUMBER 10     /* Maximum number of clients connected to the server */
#define MAX_CHANNEL_NUMBER 10    /* Maximum number of channels on the server */
#define MAX_USER_BY_CHANNEL 10   /* Maximum number of clients per channel */

static unsigned int clients_number = 0;  /* counts the client connected to the server */
static int id = 0;                       /* id of the client */
static unsigned int channels_number = 0; /* counts the defined channels */
static int socket_descriptor;            /* socket descriptor */


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

/* Channel structure */
typedef struct {
  char name[MAX_NAME_SIZE];                   /* Channel name */
  int id;                                     /* Channel index */
  int client_number;                          /* Number of user on the channel */
  client *chan_clients[MAX_USER_BY_CHANNEL];  /* Array of client */
} channel;


client *clients[MAX_CLIENT_NUMBER];
channel *channels[MAX_CHANNEL_NUMBER];

/*--------- Functions ---------*/

/* Send a message to all clients */
void send_message_to_all(char *msg){
  int i;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (clients[i]) {
      if ((write(clients[i]->cli_co, msg, strlen(msg)+1)) < 0){
	perror("error: failing to send message to clients");
      }
    }
  }
}

/* Send a message to the client given by cli_co */
void send_message_to_client(char *msg, int cli_co){
  if (write(cli_co, msg, strlen(msg)+1) < 0){
    perror("error: failing to send message to client");
  }
}

/* Send a message to the clients in a specific channel */
void send_message_to_channel(char *msg, int index){
  int i;
  for (i = 0; i < MAX_USER_BY_CHANNEL; i++){
    if (channels[index]->chan_clients[i]) {
      if ((write(channels[index]->chan_clients[i]->cli_co, msg, strlen(msg)+1)) < 0){
	perror("error: failing to send message to clients");
      }
    }
  }
}


/* Find a client in the list using the name given,
return client cli_co if found
or -1 if name is not found */
int find_client_by_name(char *name){
  int i;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (clients[i]) {
      /* Compare client name with the name given,
	 srcmp == 0 if the arguments are equal */
      if (!strcmp(clients[i]->name, name)){
	/* Return client cli_co if found */
	return clients[i]->cli_co;
      }
    }
  }
  return -1;
}

/* Enable the handling of signals */
void signal_handler(int signal_number){
  int i;
  printf("Received signal: %s\n", strsignal(signal_number));
  /* Warn the clients that the server is closing */
  if (signal_number == SIGINT) {
      for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
	if (clients[i]) {
	  send_message_to_client("Server disconnected.\n", clients[i]->cli_co);
	  close(clients[i]->cli_co);
	  free(clients[i]);
	}
      }
    close(socket_descriptor);
  }
  exit(signal_number);
}

/* Add a client to the client list and increase the number of clients */
void add_client(client *cli){
  int i;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (!clients[i]) {
      clients[i] = cli;
      break;
    }
  }
  clients_number++;
}

/* Remove a client from the client list and decrease the number of clients */
void remove_client(client *cli){
  int i;
  int cli_id = cli->id;
  for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
    if (clients[i]) {
      if ((clients[i])->id == cli_id) {
	clients[i] = NULL;
	break;
      }
    }
  }
  clients_number--;
}

int find_channel_by_name(char *chan_name){
  int i;
  for (i = 0; i < MAX_CHANNEL_NUMBER; i++) {
    if (channels[i]) {
      if (!strcmp(channels[i]->name, chan_name)){
	/* Debug */
	printf("find channel by name = %d\n", i);
	printf("chan id %d\n", channels[i]->id);
	/* Return index if found */
	return channels[i]->id;
      }
    }
  }
  return -1;
}

int add_channel(char *chan_name){
  int i;
  channel *chan;
    for (i = 0; i < MAX_CHANNEL_NUMBER; i++) {
      if (!channels[i]) {
	chan = (channel *)malloc(sizeof(channel));
	strcpy(chan->name,chan_name);
	chan->id = i;
	chan->client_number = 0;
	channels[i] = chan;
	break;
      }
    }
    channels_number++;
    return i;
}

void remove_channel(int index){
  free(channels[index]);
  channels[index] = NULL;
  channels_number--;
}

int is_client_on_channel(char *name, int chan_index){
  int i;
  channel *chan = channels[chan_index];
  for (i = 0; i < MAX_USER_BY_CHANNEL ; i++){
    if (chan->chan_clients[i]){
      if (!strcmp(chan->chan_clients[i]->name, name)){
	return 0;
      }
    }
  }
  return -1;
}

int add_client_to_channel(client *cli, int chan_index){
  int i;
  channel *chan = channels[chan_index];
  if (is_client_on_channel(cli->name, chan_index) < 0) {
    for (i = 0; i < MAX_USER_BY_CHANNEL ; i++){
      if (!chan->chan_clients[i]){
	chan->chan_clients[i] = cli;
	chan->client_number++;
	return 0;
      }
    }
  }
  return -1;
}

void remove_client_from_channel(char *name, int chan_index){}

/* Handle the client thread */
void *client_loop(void *arg){
  char *buffer = calloc(BUFFER_SIZE, 1); /* message received */
  int length, /* length of the message*/
    cli_co, /* socket_descriptor of the client */
    index;
  char out[BUFFER_SIZE]; /* message that will be sent */
  char *cmd, /* command received */
    *name, /* name received */
    *args; /* arguments received */

  /* Make proper use of the arg received */
  client *cli = (client *)arg;

  /* Greet the client */

  sprintf(out, "%d has joined the chat.\n", cli->id);
  send_message_to_all(out);
  sprintf(out, "Type /help for help.\n");
  send_message_to_client(out, cli->cli_co);

  /* Handle the reception of a message */
  /* read is blocking ; so we enter the loop only if a message is received */
  while ((length = read(cli->cli_co, buffer, BUFFER_SIZE)) > 0){
    /* Add an end to the buffer */
    buffer[length] = '\0';
    /* Handle the reception of a command */
    if (buffer[0] == '/'){
      /* strtok splits string into tokens*/
      cmd = strtok(buffer, " \n");
      /* Check which command it is */
      /* Command: /nick <name> */
      if (!strcmp(cmd, "/nick")){
	/* strtok documentation : Alternativelly, a null pointer may be specified, in which case the function continues scanning where a previous successful call to the function ended. */
	name = strtok(NULL, " \n\t");
	/* test if name is NULL, so no name was given */
	if (name){
	  /* Check if the name is not already used */
	  if (find_client_by_name(name) < 0){
	    sprintf(out, "%s renamed to %s.\n", cli->name, name);
	    strcpy(cli->name, name);
	    send_message_to_all(out);
	  }
	  else {
	    sprintf(out, "%s is already in use.\n", name);
	    send_message_to_client(out, cli->cli_co);
	  }
	}
	else {
	  send_message_to_client("You must enter a name.\n", cli->cli_co);
	}
      }
      /* Command: /me <action> */
      else if (!strcmp(cmd, "/me")){
	args = strtok(NULL, "\0");
	if (args){
	  sprintf(out, "%s %s", cli->name, args);
	  send_message_to_all(out);
	}
	else {
	  send_message_to_client("You must enter an action.\n", cli->cli_co);
	}
      }
      /* Command: /pm <name> <private-message */
      else if (!strcmp(cmd, "/pm")) {
	name = strtok(NULL, " ");
	/* Check if name exists in the client list */
	if ((cli_co = find_client_by_name(name)) < 0){
	  sprintf(out, "%s is already taken.\n", name);
	}
	/* Send the private message to both sender and receiver */
	else {
	  args = strtok(NULL, "\0");
	  if (args){
	    sprintf(out, "%s sends to you: %s", cli->name, args);
	    send_message_to_client(out, cli_co);
	    sprintf(out, "You sent to %s: %s", name, args);
	  }
	  else {
	    sprintf(out, "You must enter a message.\n");
	  }
	}
	send_message_to_client(out, cli->cli_co);
      }
      /* Command: /join <channel-name> */
      else if (!strcmp(cmd, "/join")) {
	  name = strtok(NULL, " \n\t");
	  if (name){
	    /* Add the client to the defined channel if it already exists */
	    if ((index = find_channel_by_name(name)) >= 0) {
	      if (channels[index]->client_number < MAX_USER_BY_CHANNEL){
		if (add_client_to_channel(cli, index) < 0){
		  sprintf(out, "You are already on chan %s.\n", name);
		}
		else {
		  sprintf(out, "%s had joined channel %s.\n", cli->name, name);
		  send_message_to_channel(out, index);
		sprintf(out, "Welcome to channel %s. You are the n°%d arrived on this channel.\n", name, channels[index]->client_number);
		}
	      }
	      else {
		sprintf(out, "Too many users on this channel already.\n");
	      }
	    }
	    /* if the channel doesn't exists, create it */
	    /* check if there are already too many channels */
	    else if (channels_number < MAX_CHANNEL_NUMBER){
	      /* Add the client to the newly created channel */
	      index = add_channel(name);
	      add_client_to_channel(cli, index);
	      sprintf(out, "Welcome to channel %s. You are the n°%d arrived on this channel.\n", name, channels[index]->client_number);
	    }
	    else {
	      sprintf(out, "Too many channels already.\n");
	    }
	  }
	  else {
	    sprintf(out, "You must enter a channel name.\n");
	  }
	  send_message_to_client(out, cli->cli_co);
      }
      /* Command: /tell <channel-name> <message> */
      else if (!strcmp(cmd, "/tell")) {
	  name = strtok(NULL, " \n\t");
	  if (name && (index = find_channel_by_name(name)) >= 0){
	    args = strtok(NULL, "\0");
	    if (args){
	      sprintf(out, "%s said on %s: %s", cli->name, name, args);
	      send_message_to_channel(out, index);
	    }
	    else {
	      sprintf(out, "You must enter a message.\n");
	      send_message_to_client(out, cli->cli_co);
	    }
	  }
	  else if (name){
	    sprintf(out, "Channel %s doesn't exist. Create it first with /join %s.\n", name, name);
	    send_message_to_client(out, cli->cli_co);
	  }
	  else {
	    sprintf(out, "You must enter a channel name.\n");
	    send_message_to_client(out, cli->cli_co);
	  }
      }
      else if (!strcmp(cmd, "/quit")) {
	break;
      }
      /* Command: /help */
      else if (!strcmp(cmd, "/help")){
	sprintf(out, "/nick <name>\tChange your username to <name>.\n");
	strcat(out, "/me <action>\tSend the <action> to all.\n");
	strcat(out, "/pm <name> <private-message>\tSend <private-message> to <name>.\n");
	strcat(out, "/help\tPrint this message.\n");
	send_message_to_client(out, cli->cli_co);
      }
    }
    /* Message is not a command */
    else {
      sprintf(out, "%s says : %s", cli->name, buffer);
      send_message_to_all(out);
    }
  }

  /* Client quit/disconnected */

  /* Notify the clients */
  sprintf(out, "%s has left the chat.\n", cli->name);
  send_message_to_all(out);

  /* Handle the proper closing of the thread */
  close(cli->cli_co);
  remove_client(cli);
  free(cli);
  free(buffer);
  pthread_detach(pthread_self());
  return NULL;
}

/*--------- Main ---------*/

int main(int argc, char **argv) {
  int new_socket_descriptor,  /* new socket descriptor */
    address_length; /* client address length */
  sockaddr_in local_address,    /* local address socket informations */
    cli_addr;  /* client address */
  hostent* ptr_host;  /* informations about host */
  char host_name[MAX_NAME_SIZE+1];  /* host name */
  pthread_t thread; /* thread to handle client */
  client *cli; /* client structure */

  /* Handle SIGPIPE signal */
  signal(SIGPIPE, signal_handler);
  signal(SIGINT, signal_handler);

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
  printf("Using port : %d \n", ntohs(local_address.sin_port));
  /* create socket in socket_descriptor */
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


  for(;;) {
    address_length = sizeof(cli_addr);
    /* cli_addr given by accept with connect informations*/
    if ((new_socket_descriptor =
	 accept(socket_descriptor,
		(sockaddr*)(&cli_addr),
		&address_length))
	< 0) {
      perror("error: unable to accept connection to the client.");
      exit(1);
    }

    /* check if there are already too many clients */
    if ( (clients_number+1) == MAX_CLIENT_NUMBER){
      printf("Too many clients already; client rejected\n");
      close(new_socket_descriptor);
    }

    /* Client settings and handling */
    cli = (client *)malloc(sizeof(client));
    cli->addr = cli_addr;
    cli->cli_co = new_socket_descriptor;
    cli->id = id++;
    sprintf(cli->name, "%d", cli->id);
    printf("Client connected, using the id: %d\n", cli->id);

    add_client(cli);
    pthread_create(&thread, NULL, client_loop, (void *)cli);

  }


  return EXIT_SUCCESS;
}
