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

/* #define DEBUG 1 */

#include "include/qaoed.h"
#include "include/logging.h"
#include "include/api.h"

int processSTATUSrequest(int conn,struct apihdr *api_hdr,void *arg)
{
  struct qaoed_status status;
  
  status.num_targets = 0;
  status.num_interfaces = 0;
  status.num_threads = 0;

  api_hdr->type = REPLY;
  api_hdr->error = API_ALLOK;
  api_hdr->arg_len = sizeof(struct qaoed_status);

  send(conn,api_hdr,sizeof(struct apihdr),0);
  send(conn,&status,sizeof(struct qaoed_status),0);

  return(0); 
}

int processTARGET_DEL(struct qconfig *conf, int conn,
		      struct apihdr *api_hdr, 
		      struct qaoed_target_cmd *cmd)
{
   struct aoedev *device;

   /* Search for matching device */
   /* FIXME: what if more then one target matches the search??? */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == cmd->slot  &&
	 device->shelf == cmd->shelf))
       {
	  /* Unlink device from device-list */
	  /* eh... */
	  return(-1); /* Not done yet */
	  
	  if(pthread_cancel(device->threadID) != 0)
	    {
	       logfunc(conf->log,LOG_ERR,"Failed to stop device thread for %s\n",
		       device->devicename);
	       return(-1);
	    }
	  else
	    return(0);
       }
      
   return(-1);
}


int processTARGET_ADD(struct qconfig *conf, int conn, 
		      struct apihdr *api_hdr, 
		      struct qaoed_target_cmd *cmd)
{
  struct aoedev *device;
  struct aoedev *newdevice;
  newdevice = (struct aoedev *) malloc(sizeof(struct aoedev));
  
  if(newdevice == NULL)
    {
      return(-1);
    }

#ifdef DEBUG
  printf("Processing new device!\n");
  fflush(stdout);
#endif

  newdevice->devicename = strdup(cmd->devicename);
  newdevice->shelf = cmd->shelf;
  newdevice->slot = cmd->slot;
  newdevice->broadcast = cmd->broadcast;
  newdevice->writecache = cmd->writecache;
  if(strlen(cmd->ifname) > 0)
    newdevice->interface = referenceint(cmd->ifname,conf);
  else
    newdevice->interface = NULL;
  newdevice->next       = NULL;
  newdevice->log        = NULL;
  newdevice->fp         = NULL;

  /* Set default cfg lenght value */
  newdevice->cfg_len = 0;
  
  /* Set default ACL (none that is) */
  newdevice->wacl      = NULL;
  newdevice->racl      = NULL;
  newdevice->cfgsetacl = NULL;
  newdevice->cfgracl   = NULL;
  
  /* Assign default values from the default target */
  qaoed_devdefaults(conf,newdevice);

  /* Make sure we dont set the same slot/shelf on multiple targets
   * unless they point to the same device and uses different interfaces */
  for(device = conf->devices; device != NULL; device = device->next)
      if((device->slot == newdevice->slot  &&
	  device->shelf == newdevice->shelf))
	if((device->interface == newdevice->interface) ||
	   (strcmp(device->devicename,newdevice->devicename) != 0))
	  {
	    /* Two devices can only share the same slot/shelf if 
	     * they both point to the same target and are attached 
	     * to different interfaces... */
	    free(newdevice);
	    return(-1);
	  }

  /* Try to start our device */
  if(qaoed_startdevice(newdevice) == 0)
    {
      /* Find end of device list */
      for(device=conf->devices; device->next!=NULL; device=device->next);;
      
      /* Add our device */
      device->next  = newdevice;
      
#ifdef DEBUG
      printf("New device sucessfully started!\n");
      fflush(stdout);
#endif
      
      cmd->shelf = newdevice->shelf;
      cmd->slot = newdevice->slot;
      cmd->broadcast = newdevice->broadcast;
      cmd->writecache = newdevice->writecache;
      strcpy(cmd->devicename, newdevice->devicename);
      strcpy(cmd->ifname, newdevice->interface->ifname);

      return(0);
    }
  else
    {
#ifdef DEBUG
  printf("New device failed to start!\n");
  fflush(stdout);
#endif

      return(-1);
    }

  return(0);
}

int processTARGETrequest(struct qconfig *conf, int conn,
			 struct apihdr *api_hdr,void *arg)
{
  api_hdr->error = API_ALLOK;

  struct qaoed_target_cmd *cmd = (struct qaoed_target_cmd *) arg;

   switch(api_hdr->cmd)
     {
      case API_CMD_TARGETS:
	printf("API_CMD_TARGETS - no function ");
	break;
	
      case API_CMD_TARGETS_STATUS:
	printf("API_CMD_TARGETS_STATUS - no function \n");
	break;
	
     case API_CMD_TARGETS_ADD:
#ifdef DEBUG
	printf("API_CMD_TARGETS_ADD -- processing\n");
#endif
       if(processTARGET_ADD(conf, conn,api_hdr,cmd) == -1)
	 api_hdr->error = API_FAILURE;
       break;
	
      case API_CMD_TARGETS_DEL:
#ifdef DEBUG
	printf("API_CMD_TARGETS_DEL -- processing \n");
#endif
	if(processTARGET_DEL(conf, conn,api_hdr,cmd) == -1)
	 api_hdr->error = API_FAILURE;
	break;
	
      case API_CMD_TARGETS_SETOPTION:
	printf("API_CMD_TARGETS_SETOPTION - no function \n");
	break;
	
      case API_CMD_TARGETS_SETACL:
	printf("API_CMD_TARGETS_SETACL - no function \n");
	break;

     }

  api_hdr->type = REPLY;
  api_hdr->arg_len = sizeof(struct qaoed_target_cmd);
  
  printf("api_hdr->error: %d\n",api_hdr->error);
  printf("api_hdr->cmd: %d\n",api_hdr->cmd);
  printf("api_hdr->type: %d\n",api_hdr->type);
  printf("api_hdr->arg_len: %d\n",api_hdr->arg_len);

  send(conn, api_hdr, sizeof(struct apihdr)          , 0);
  send(conn, cmd,     sizeof(struct qaoed_target_cmd), 0);

  return(0); 
}


int processAPIrequest(struct qconfig *conf, int conn,
		      struct apihdr *api_hdr,void *arg)
{
   printf("recieved request of type: ");
   switch(api_hdr->cmd)
     {
      case API_CMD_STATUS:
	printf("API_CMD_STATUS\n");
	processSTATUSrequest(conn,api_hdr,arg);
	break;
	
      case API_CMD_INTERFACES:
	printf("API_CMD_INTERFACES\n");
	break; 
	
      case API_CMD_INTERFACES_STATUS:
	printf("API_CMD_INTERFACES_STATUS\n");
	break;
	
      case API_CMD_INTERFACES_ADD:
	printf("API_CMD_INTERFACES_ADD\n");
	break;
	
      case API_CMD_INTERFACES_DEL:
	printf("API_CMD_INTERFACES_DEL\n");
	break;
	
      case API_CMD_INTERFACES_SETMTU:
	printf("API_CMD_INTERFACES_SETMTU\n");
	break;
	
      case API_CMD_TARGETS:
	printf("API_CMD_TARGETS");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGETS_STATUS:
	processTARGETrequest(conf, conn,api_hdr,arg);
	printf("API_CMD_TARGETS_STATUS\n");
	break;
	
      case API_CMD_TARGETS_ADD:
	printf("API_CMD_TARGETS_ADD\n");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGETS_DEL:
	printf("API_CMD_TARGETS_DEL\n");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGETS_SETOPTION:
	printf("API_CMD_TARGETS_SETOPTION\n");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGETS_SETACL:
	printf("API_CMD_TARGETS_SETACL\n");
	break;
	
      case API_CMD_ACL:
	printf("API_CMD_ACL\n");
	break;
	
      case API_CMD_ACL_STATUS:
	printf("API_CMD_ACL_STATUS\n");
	break;
      case API_CMD_ACL_ADD:
	printf("API_CMD_ACL_ADD\n");
	break;
      case API_CMD_ACL_DEL:
	printf("API_CMD_ACL_DEL\n");
	break;
	
      default:
	printf("UNKNOWN!\n");
	break;
     }

   return(0);
}


int recvAPIrequest(int conn, struct qconfig *conf)
{
   struct apihdr api_hdr;
   char *arg = NULL;
   int n;

#ifdef DEBUG
   printf("recvAPIrequest()\n");
   fflush(stdout);
#endif

   while(1)
     {
	arg = NULL;
	
	/* Read api_hdr */
	n = recv(conn, &api_hdr, sizeof(struct apihdr),0);
	
	/* Make sure we got data of the correct size */
	if (n != sizeof(struct apihdr))
	  {
#ifdef DEBUG
	    if(n > 0)
	      printf("Wrong size for api_hdr: %d\n",n);
#endif

	     close(conn);
	     return(-1);
	  }
	
	/* Is there more data ? */
	if(api_hdr.arg_len > 0)
	  {
	     /* If its way out of proportions we close the socket */
	     if(api_hdr.arg_len > 2048)
	       {
#ifdef DEBUG
		 printf("argument lenght has bogus value: %d\n",api_hdr.arg_len);
#endif
		  close(conn);
		  return(-1);
	       }
	     
	     /* Allocate memory to hold options */
	     arg = (char *) malloc(api_hdr.arg_len);
	     if(arg == NULL)
	       {
#ifdef DEBUG
		 printf("malloc failed!\n");
#endif
		  close(conn);
		  return(-1);
	       }
	     
	     /* Read option arguments */
	     n = recv(conn, arg, api_hdr.arg_len,0);
	     
	     /* Make sure we got data of the right size */
	     if (n != api_hdr.arg_len)
	       {
#ifdef DEBUG
		 printf("Bogus lenght recieved from socket: %d\n",n);
#endif 
		  close(conn);
		  return(-1);
	       }
	  
	  }
	
	/* Process the request */
	processAPIrequest(conf, conn,&api_hdr,arg);
	
	/* Free memory used by args */
	if(arg != NULL)
	  free(arg);
	
     }
}



/* This is the API thread for qaoed */
int qaoed_api(struct qconfig *conf)
{
   int sock,conn;
   struct sockaddr_un lsock, rsock;
   socklen_t sockaddr_len;
   
   if(conf->sockpath == NULL)
     return(-1);

#ifdef DEBUG
   printf("qaoed_api() starting\n");
#endif
   
   /* Remove old socket file */
   unlink(conf->sockpath);
   
   /* Create a socket */
   if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
     {
#ifdef DEBUG
       fprintf(stdout,"Failed to create socket for API interface: socket() - %s",
		strerror(errno));
       fflush(stdout);

#endif
	logfunc(conf->log,LOG_ERR,
		"Failed to create socket for API interface: socket() - %s",
		strerror(errno));
	return(-1);
     }
   
   /* Fill out the sockaddr_un struct */
   lsock.sun_family = AF_UNIX;
   strcpy(lsock.sun_path, conf->sockpath);
   sockaddr_len = strlen(conf->sockpath) + sizeof(lsock.sun_family) + 1;

#ifdef DEBUG
   printf("Binding the socket\n");
#endif

   /* Bind the socket */
   if (bind(sock, (struct sockaddr *)&lsock, sockaddr_len) == -1)
     {
#ifdef DEBUG
       fprintf(stdout,"Failed to bind socket for API interface: bind() - %s\n",
	       strerror(errno));
       fflush(stdout);
#endif
	logfunc(conf->log,LOG_ERR,
		"Failed to bind socket for API interface: bind() - %s",
		strerror(errno));
	return(-1);
     }

#ifdef DEBUG
   printf("Listening for connections\n");
#endif

   if (listen(sock, 5) == -1) 
     {
#ifdef DEBUG
       fprintf(stdout,"Failed to listen on socket for API interface: listen()"
	       "- %s\n",
	       strerror(errno));
       fflush(stdout);
#endif
       logfunc(conf->log,LOG_ERR,
	       "Failed to listen on socket for API interface: listen() - %s",
	       strerror(errno));
       return(-1);
     }
   
#ifdef DEBUG
   printf("accepting connections\n");
#endif

   sockaddr_len =  sizeof(lsock.sun_family);
   while((conn = accept(sock, (struct sockaddr *)&rsock, &sockaddr_len)) >= 0)
     recvAPIrequest(conn,conf);
   
   if(conn < 0)
     {

#ifdef DEBUG
       fprintf(stdout,
	       "Failed to accept() on socket for API interface- %s\n",
	       strerror(errno));
       fflush(stdout);
#endif
	logfunc(conf->log,LOG_ERR,
		"Failed to accept() on socket for API interface: accept() - %s",
		strerror(errno));
	return(-1);
     }
   
#ifdef DEBUG
   printf("This should never happen!\n");
#endif

   /* Ehh? */
   return(0);
}

   

int qaoed_startapi(struct qconfig *conf)
{
   pthread_attr_t atr;
   
   if(conf == NULL)
     return(-1);
   
#ifdef DEBUG
   printf("qaoed_startapi()\n");
#endif

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


