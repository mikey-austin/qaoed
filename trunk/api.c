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
#include "include/acl.h"

/* Return some statistics about qaoed */
int processSTATUSrequest(struct qconfig *conf, int conn,
			 struct apihdr *api_hdr,void *arg)
{
   struct qaoed_status status;
   struct aoedev *device;
   struct ifst *ifent;
   
   status.num_targets    = 0;
   status.num_interfaces = 0;
   status.num_threads    = 0;
   
   /* Place a readlock on the device-list before counting */
   pthread_rwlock_rdlock(&conf->devlistlock);
   /* Count the number of device threads */
   for(device = conf->devices; device != NULL; device = device->next)
     status.num_targets++;
   /* unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   /* Count the number of interfaces */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     status.num_interfaces++;
   
   /* Sum up the information */
   status.num_threads = status.num_interfaces + status.num_targets + 2;
   
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = sizeof(struct qaoed_status);
   
   send(conn,api_hdr,sizeof(struct apihdr),0);
   send(conn,&status,sizeof(struct qaoed_status),0);
   
   return(0); 
}

/* Return information about targets */
int processTARGET_LIST(struct qconfig *conf, int conn,
		       struct apihdr *api_hdr)
{
   struct aoedev *device;
   struct qaoed_target_info *tglist;
   struct qaoed_target_info *tg;
   int repsize = 0;
   int cnt = 0; 
   
   /* Place a readlock on the device-list before counting */
   pthread_rwlock_rdlock(&conf->devlistlock);
   /* Count the number of device threads */
   for(device = conf->devices; device != NULL; device = device->next)
     cnt++;
   
   repsize = cnt * sizeof(struct qaoed_target_info);
   
   tg = tglist = (struct qaoed_target_info *) malloc(repsize);
   
   if(tglist == NULL)
     {
	/* unlock */
	pthread_rwlock_unlock(&conf->devlistlock);
	return(-1);
     }
   
   for(device = conf->devices; device != NULL; device = device->next)
     {
	/* Make sure we dont send more data then we have room for */
	if(cnt-- <= 0)
	  break;
	
	tg->shelf = device->shelf;
	tg->slot = device->slot;
	tg->broadcast = device->broadcast;
	tg->writecache = device->writecache;
	strcpy(tg->devicename, device->devicename);
	strcpy(tg->ifname, device->interface->ifname);
	
	/* Move to the next */
	tg++;
     }
   
   
   /* unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = repsize;
   
   send(conn,api_hdr,sizeof(struct apihdr),0);
   send(conn,tglist, repsize,              0);
   
   return(0); 
}

/* Return a list of access-lists */
int processACL_LIST(struct qconfig *conf, int conn,
		    struct apihdr *api_hdr)
{
   struct aclhdr *acllist;
   struct qaoed_acl_info *aclinfo;
   struct qaoed_acl_info *anfo;
   int repsize = 0;
   int cnt; 
   
   /* Count the number access-lists */
   for(acllist = conf->acllist; acllist != NULL; acllist = acllist->next)
     cnt++;

   /* Calc size and allocate memory */
   repsize = cnt * sizeof(struct qaoed_acl_info);
   anfo = aclinfo = (struct qaoed_acl_info *) malloc(repsize);
   
   if(aclinfo == NULL)
	return(-1);
   
   /* Extract info */
   for(acllist = conf->acllist; acllist != NULL; acllist = acllist->next)
     {
	/* Make sure we dont send more data then we have room for */
	if(cnt-- <= 0)
	  break;
	
	anfo->aclnumber = acllist->aclnum;
	anfo->defaultpolicy = acllist->defaultpolicy;
	strcpy(anfo->name, acllist->name);
	
	/* Move to the next */
	anfo++;
     }
   
   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = repsize;
   
   /* Send it */
   send(conn,api_hdr, sizeof(struct apihdr),0);
   send(conn,aclinfo, repsize,              0);
   
   return(0); 
}

int processTARGET_DEL(struct qconfig *conf, int conn,
		      struct apihdr *api_hdr, 
		      struct qaoed_target_info *target)
{
   struct aoedev *device;
   int ret = 0;
   int cnt = 0;
   
   /* Place a writelock on the device-list before modifying */
   pthread_rwlock_wrlock(&conf->devlistlock);
   
   /* Make sure that we wont get more then one hit in the search */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == target->slot  &&
	 device->shelf == target->shelf))
       if(target->ifname[0] == 0 ||
	  device->interface == referenceint(target->ifname,conf))
	 cnt++;
   
   /* unlock the device-list */
   if(cnt > 1)
     {
	/* More then one device matched the delete request */
	pthread_rwlock_unlock(&conf->devlistlock);
	return(-1);
     }
      
   /* Search for matching device */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == target->slot  &&
	 device->shelf == target->shelf))
       if(target->ifname[0] == 0 ||
	  device->interface == referenceint(target->ifname,conf))
	 {
	    /* Unlink device from device-list */
	    if(device->prev != NULL)
	      device->prev->next = device->next;
	    if(device->next != NULL)
	      device->next->prev = device->prev;
	    
	    /* Try to stop thread */
	    ret = pthread_cancel(device->threadID);
	    
	    if(ret == -1)
	      logfunc(conf->log,
		      LOG_ERR,
		      "Failed to stop device thread for %s\n",
		      device->devicename);
	 }
   
   /* Unlock write lock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   return(ret);
}


int processTARGET_ADD(struct qconfig *conf, int conn, 
		      struct apihdr *api_hdr, 
		      struct qaoed_target_info *target)
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

  newdevice->devicename = strdup(target->devicename);
  newdevice->shelf = target->shelf;
  newdevice->slot = target->slot;
  newdevice->broadcast = target->broadcast;
  newdevice->writecache = target->writecache;
  if(strlen(target->ifname) > 0)
    newdevice->interface = referenceint(target->ifname,conf);
  else
    newdevice->interface = NULL;
  newdevice->next       = NULL;
  newdevice->prev       = NULL;
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

   /* Read-lock */
   pthread_rwlock_rdlock(&conf->devlistlock);
   
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
	     /* Unlock before return */
	     pthread_rwlock_unlock(&conf->devlistlock);
	     return(-1);
	  }

   /* Unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
  /* Try to start our device */
  if(qaoed_startdevice(newdevice) == 0)
    {
       /* Place a writelock on the device-list before modifying */
       pthread_rwlock_wrlock(&conf->devlistlock);
       
       /* Find end of device list */
       for(device=conf->devices; device->next!=NULL; device=device->next);;
       
       /* Add our device */
       device->next  = newdevice;
       newdevice->prev = device;
       
       /* Unlock */
       pthread_rwlock_unlock(&conf->devlistlock);
       
#ifdef DEBUG
      printf("New device sucessfully started!\n");
      fflush(stdout);
#endif
      
      target->shelf = newdevice->shelf;
      target->slot = newdevice->slot;
      target->broadcast = newdevice->broadcast;
      target->writecache = newdevice->writecache;
      strcpy(target->devicename, newdevice->devicename);
      strcpy(target->ifname, newdevice->interface->ifname);

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

   struct qaoed_target_info *target = (struct qaoed_target_info *) arg;

   switch(api_hdr->cmd)
     {
      case API_CMD_TARGET_LIST:
	printf("API_CMD_TARGET_LIST - processing\n ");
	if(processTARGET_LIST(conf, conn,api_hdr) == -1)
	  api_hdr->error = API_FAILURE;
	break;
	
      case API_CMD_TARGET_STATUS:
	printf("API_CMD_TARGET_STATUS - no function \n");
	break;
	
     case API_CMD_TARGET_ADD:
#ifdef DEBUG
	printf("API_CMD_TARGET_ADD -- processing\n");
#endif
       if(processTARGET_ADD(conf, conn,api_hdr,target) == -1)
	 api_hdr->error = API_FAILURE;
       break;
	
      case API_CMD_TARGET_DEL:
#ifdef DEBUG
	printf("API_CMD_TARGET_DEL -- processing \n");
#endif
	if(processTARGET_DEL(conf, conn,api_hdr,target) == -1)
	 api_hdr->error = API_FAILURE;
	break;
	
      case API_CMD_TARGET_SETOPTION:
	printf("API_CMD_TARGET_SETOPTION - no function \n");
	break;
	
      case API_CMD_TARGET_SETACL:
	printf("API_CMD_TARGET_SETACL - no function \n");
	break;

     }

  api_hdr->type = REPLY;
  api_hdr->arg_len = sizeof(struct qaoed_target_info);
  
  printf("api_hdr->error: %d\n",api_hdr->error);
  printf("api_hdr->cmd: %d\n",api_hdr->cmd);
  printf("api_hdr->type: %d\n",api_hdr->type);
  printf("api_hdr->arg_len: %d\n",api_hdr->arg_len);

  send(conn, api_hdr, sizeof(struct apihdr)          , 0);
  send(conn, target,  sizeof(struct qaoed_target_info), 0);

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
	processSTATUSrequest(conf,conn,api_hdr,arg);
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
	
      case API_CMD_TARGET_LIST:
	printf("API_CMD_TARGET_LIST");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_STATUS:
	processTARGETrequest(conf, conn,api_hdr,arg);
	printf("API_CMD_TARGET_STATUS\n");
	break;
	
      case API_CMD_TARGET_ADD:
	printf("API_CMD_TARGET_ADD\n");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_DEL:
	printf("API_CMD_TARGET_DEL\n");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_SETOPTION:
	printf("API_CMD_TARGET_SETOPTION\n");
	processTARGETrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_SETACL:
	printf("API_CMD_TARGET_SETACL\n");
	break;
	
      case API_CMD_ACL_LIST:
	printf("API_CMD_ACL - Processing\n");
	processACL_LIST(conf, conn,api_hdr);
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


