#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#include "include/qaoed.h"
#include "include/logging.h"

/* This is the API thread for qaoed */
int qaoed_api(struct qconfig *conf)
{
   int sock,conn,len;
   struct sockaddr_un lsock, rsock;
   int sockaddr_len;
   
   if(conf->sockpath == NULL)
     return(-1);
   
   /* Remove any old socket file */
   unlink(lsock.sun_path);
   
   /* Create a socket */
   if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
     {
	logfunc(conf->log,LOG_ERR,
		"Failed to create socket for API interface: socket() - %s",
		strerror(errno));
	return(-1);
     }
   
   /* Fill out the sockaddr_un struct */
   lsock.sun_family = AF_UNIX;
   strcpy(lsock.sun_path, conf->sockpath);
   sockaddr_len = strlen(conf->sockpath) + sizeof(lsock.sun_family);

   /* Bind the socket */
   if (bind(sock, (struct sockaddr *)&lsock, sockaddr_len) == -1)
     {
	logfunc(conf->log,LOG_ERR,
		"Failed to bind socket for API interface: bind() - %s",
		strerror(errno));
	return(-1);
     }
   
   if (listen(sock, 5) == -1) 
     {
	logfunc(conf->log,LOG_ERR,
		"Failed to listen on socket for API interface: listen() - %s",
		strerror(errno));
	return(-1);
     }
   
   while((conn = accept(sock, (struct sockaddr *)&rsock, &len)) >= 0)
     {
	
	
	
     }
   
   if(conn < 0)
     {
	logfunc(conf->log,LOG_ERR,
		"Failed to accept() on socket for API interface: accept() - %s",
		strerror(errno));
	return(-1);
     }
   
   
   /* Ehh? */
   return(0);
}

   

int qaoed_startapi(struct qconfig *conf)
{
   pthread_attr_t atr;
   
   if(conf == NULL)
     return(-1);
   
   /* Initialize the attributes */
   pthread_attr_init(&atr);
   pthread_attr_setdetachstate(&atr, PTHREAD_CREATE_JOINABLE);
	
   /* Start the API thread */
   if(pthread_create (&conf->APIthreadID,
		      &atr,
		      (void *)&qaoed_api,(void *)conf) != 0)
     {
	logfunc(conf->log,LOG_ERR,"Failed to start API interface for qaoed\n");
	return(-1);
     }	
		
   
   /* Return success status */
   return(0);
   
}


