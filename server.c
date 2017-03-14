
/*----------------------------------------------
  Server-side application
  ------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/types.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>


/*--------- Define constants and global variables ---------*/

#define SERVER_PORT 5000         /* Port used for sin_port from sockaddr_in */
#define BUFFER_SIZE 1024          /* Size of buffers used */
#define MAX_NAME_SIZE 32        /* Maximum name size for users and channels */
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

/* Return a formatted list of users of the server */
char* who_is_on_server(){
  int i;
  char *list = calloc(BUFFER_SIZE, 1);
  char name[MAX_NAME_SIZE + 2];
  for (i = 0; i < MAX_CLIENT_NUMBER ; i++){
    if (clients[i]){
      sprintf(name, "%s ", clients[i]->name);
      strcat(list, name);
    }
  }
  return list;
}

/* Find a channel in channels array given its name
   Return the index where the channel is in the array, -1 if not found */
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

/* Add a channel to the channels array
   Return the index where it was added */
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

/* Removes a channel from the channels array, return the number of channels left */
int remove_channel(int index){
  free(channels[index]);
  channels[index] = NULL;
  channels_number--;
  return channels_number;
}

/* Say if a user (from his name) is on a chan given its position in channels array
   Return 0 if the given */
int is_user_on_channel(char *name, int chan_index){
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

/* Add a client to a channel given its position in channels array.
   Return the index of the client in the array, or -1 if the user is already on the channel*/
int add_client_to_channel(client *cli, int chan_index){
  int i;
  channel *chan = channels[chan_index];
  if (is_user_on_channel(cli->name, chan_index) < 0) {
    for (i = 0; i < MAX_USER_BY_CHANNEL ; i++){
      if (!chan->chan_clients[i]){
	chan->chan_clients[i] = cli;
	chan->client_number++;
	return i;
      }
    }
  }
  return -1;
}

/* Remove a user (from his name) from a chan given its position in channels
   Return the number of user left on the channel. */
int remove_user_from_channel(char *name, int chan_index){
  int i;
  channel *chan = channels[chan_index];
  for (i = 0; i < MAX_USER_BY_CHANNEL ; i++){
    if (chan->chan_clients[i] && !strcmp(chan->chan_clients[i]->name, name)) {
      chan->chan_clients[i] = NULL;
      chan->client_number--;
      break;
    }
  }
  return chan->client_number;
}


/* Return a formatted list of users of a channel given its position in channels */
char* who_is_on_channel(int chan_index){
  int i;
  channel *chan = channels[chan_index];
  char *list = calloc(BUFFER_SIZE, 1);
  char name[MAX_NAME_SIZE + 2];
  for (i = 0; i < MAX_USER_BY_CHANNEL ; i++){
    if (chan->chan_clients[i]){
      sprintf(name, "%s ", chan->chan_clients[i]->name);
      strcat(list, name);
    }
  }
  return list;
}

/* Handle the client thread */
void *client_loop(void *arg){
  char *buffer = calloc(BUFFER_SIZE, 1); /* message received */
  int length, /* length of the message*/
    cli_co, /* socket_descriptor of the client */
    index,
    answer;
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
	  /* Send message if the given name is a channel */
	  if (name && (index = find_channel_by_name(name)) >= 0){
	    args = strtok(NULL, "\0");
	    /* And if there is a message */
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
      /* Command: /leave <channel-name> */
      else if (!strcmp(cmd, "/leave")) {
	name = strtok(NULL, " \n\t");
	if (name){
	  /* Get the index of the chan given */
	  index = find_channel_by_name(name);
	  /* Remove the user only if he is already on channel */
	  if (is_user_on_channel(cli->name, index) == 0) {
	    answer = remove_user_from_channel(cli->name, index);
	    sprintf(out, "Left channel: %s. \n", name);
	    send_message_to_client(out, cli->cli_co);
	    sprintf(out, "%s left channel %s.\n", cli->name, name);
	    send_message_to_channel(out, index);
	    /* Remove chan from the chan list if there are not any user left */
	    if (answer == 0) {
	      printf("number of chan: %d.\n", channels_number);
	      remove_channel(index);
	      printf("number of chan: %d.\n", channels_number);
	    }
	  }
	}
      }
      /* Command: /who <channel> */
      else if (!strcmp(cmd, "/who")) {
	args = strtok(NULL, " \n\t");
	if (args){
	  /* If global, list the users on the server */
	  if (!strcmp(args, "global")){
	    name = who_is_on_server();
	    sprintf(out, "Users on the server: %s\n", name);
	    free(name);
	  }
	  /* If not and the args are a channel-name, list the users on the channel */
	  else if ((index = find_channel_by_name(args)) >= 0){
	    name = who_is_on_channel(index);
	    sprintf(out, "Users on channel %s: %s\n", args, name);
	    free(name);
	  }
	  else {
	    sprintf(out, "No channel named %s.\n", args);
	  }
	}
	send_message_to_client(out, cli->cli_co);
      }
      /* Command: /howmany <channel> */
      else if (!strcmp(cmd, "/howmany")) {
	args = strtok(NULL, " \n\t");
      	if (args){
	  /* If global, return the number of users on the server */
	  if (!strcmp(args, "global")){
	    sprintf(out, "Users on the server: %d on %d users authorized.\n",
		    clients_number, MAX_CLIENT_NUMBER);
	  }
	  /* If channels, return the number of channels used */
	  else if (!strcmp(args, "channels")){
	    sprintf(out, "%d channels out of %d available", channels_number, MAX_CHANNEL_NUMBER);
	  }
	  /* If not and the args are a channel-name, return the number of users on the channel */
	  else if ((index = find_channel_by_name(args)) >= 0){
	    sprintf(out, "Users on channel %s : %d on %d users authorized.\n",
		    channels[index]->name, channels[index]->client_number, MAX_USER_BY_CHANNEL);
	  }
	  else {
	    sprintf(out, "No channel named %s.\n", args);
	  }
	  send_message_to_client(out, cli->cli_co);
	}
      }
      /* Command: /quit */
      else if (!strcmp(cmd, "/quit")) {
	break;
      }
      /* Command: /help or not recognized command */
      else {
	sprintf(out, "\n");
	if (strcmp(cmd, "/help")){
	  strcat(out, "Unrecognized command.\n");
	}
	strcat(out, "/nick <name>\tChange your username to <name>.\n");
	strcat(out, "/me <action>\tSend the <action> to all.\n");
	strcat(out, "/pm <name> <private-message>\tSend <private-message> to <name>.\n");
	strcat(out, "/join <channel-name>\tJoin or create channel <channel-name>.\n");
	strcat(out, "/tell <channel-name> <message>\tSend a message to a previously created channel.\n");
	strcat(out, "/leave <channel-name>\tLeave channel <channel-name>.\n");
	strcat(out, "/who <channel>\tList the users on <channel>. Use 'global' for server.\n");
	strcat(out, "/howmany <channel>\tCounts the users on <channel>. Use 'global' for server.\n");
	strcat(out, "/quit\tQuit the client.\n");
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
