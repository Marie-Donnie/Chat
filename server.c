
/*----------------------------------------------
  Server-side application
  ------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

/*--------- Define struct types to use it more easily ---------*/

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;
typedef struct servent servent;

/*--------- Define constants ---------*/

#define TAILLE_MAX_NOM 256
