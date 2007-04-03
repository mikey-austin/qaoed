#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "include/api.h"

#define SOCK_PATH "/tmp/qaoedsocket"

int processSTATUSreply(int sock,struct apihdr *api_hdr,void *arg)
{
  struct qaoed_status *status = (struct qaoed_status *) arg;
  
  if(api_hdr->cmd == API_CMD_STATUS && 
     api_hdr->arg_len == sizeof(struct qaoed_status))
    {
      printf("Qaoed status: \n");
      printf("   Running threads:   %d\n",status->num_threads);
      printf("   exported targets:  %d\n",status->num_targets);
      printf("   open interfaces:   %d\n",status->num_interfaces);
    }

  return(0);
}

int processTARGETADDreply(int sock,struct apihdr *api_hdr,void *arg)
{
  struct qaoed_target_info *target = (struct qaoed_target_info *) arg;
  
  if(api_hdr->cmd == API_CMD_TARGET_ADD && 
     api_hdr->arg_len == sizeof(struct qaoed_target_info))
    {
      if(api_hdr->error == API_ALLOK)
	printf("Successfully added new target\n");
      else
	printf("Failed to add new target\n");
	  printf("Target: %s\n",target->devicename);
	  printf("Shelf:  %d\n",target->shelf);
	  printf("Slot:   %d\n",target->slot);
	  printf("Interface: %s\n",target->ifname);
	  printf("Broadcast: %d\n",target->broadcast);
	  printf("Writecache: %d\n",target->writecache);
    }
  else
    printf("Failed to understand reply from server\n");
  return(0);
}

int processACLLISTreply(int sock,struct apihdr *api_hdr,void *arg)
{
   int cnt; 
   struct qaoed_acl_info *aclinfo = (struct qaoed_acl_info *) arg;
   
   cnt = api_hdr->arg_len / sizeof(struct qaoed_acl_info);
   
   while(cnt--)
     {
	printf("acl -- %s\n",aclinfo->name);
	aclinfo++;
     }
   
   return(0);
}

int processTARGETLISTreply(int sock,struct apihdr *api_hdr,void *arg)
{
   int cnt; 
   struct qaoed_target_info *target = (struct qaoed_target_info *) arg;
   
   cnt = api_hdr->arg_len / sizeof(struct qaoed_target_info);

   printf("-- Device list\n");
   
   while(cnt--)
     {
	printf("file: %s    shelf: %d        slot: %d\n",target->devicename,
	       target->shelf,target->slot);
	target++;
     }
   
   return(0);
}


int processAPIreply(int sock,struct apihdr *api_hdr,void *arg)
{

  switch(api_hdr->cmd)
    {
    case API_CMD_STATUS:
      processSTATUSreply(sock,api_hdr,arg);
      break;

    case API_CMD_INTERFACES_LIST:
      printf("recieved reply for: ");
      printf("API_CMD_INTERFACES\n");
      break;

    case API_CMD_TARGET_LIST:
       processTARGETLISTreply(sock,api_hdr,arg);
      break;

    case API_CMD_TARGET_STATUS:
      printf("API_CMD_TARGET_STATUS - no function \n");
      break;

    case API_CMD_TARGET_ADD:
      processTARGETADDreply(sock,api_hdr,arg);
      break;

    case API_CMD_TARGET_DEL:
      printf("API_CMD_TARGET_DEL - no function \n");
      break;

     case API_CMD_ACL_LIST:
       processACLLISTreply(sock,api_hdr,arg);
       break;
       
    default:
      printf("Unknown cmd: %d\n",api_hdr->cmd);
    }

  return(0);
}

int recvreply(int sock)
{
  struct apihdr api_hdr;
  void *arg;
  int n;

  /* Read api_hdr */
  n = recv(sock, &api_hdr, sizeof(struct apihdr),0);

   /* Make sure we got data of the correct size */
   if (n != sizeof(struct apihdr))
     {
       if(n > 0)
	 printf("Wrong size for api_hdr: %d\n",n);
       
       close(sock);
       return(-1);
     }

   if(api_hdr.error != API_ALLOK)
     {
       printf("error!\n");
       fflush(stdout);
     }

   /* Is there more data ? */
   if(api_hdr.arg_len > 0)
     {
       /* If its way out of proportions we close the socket */
       if(api_hdr.arg_len > 2048)
	 {
	   printf("argument lenght has bogus value: %d\n",
		  api_hdr.arg_len);
	   close(sock);
	   return(-1);
	 }
       
       /* Allocate memory to hold options */
       arg = (char *) malloc(api_hdr.arg_len);
       if(arg == NULL)
	 {
	   perror("malloc");
	   close(sock);
	   return(-1);
	 }

       /* Read option arguments */
       n = recv(sock, arg, api_hdr.arg_len,0);
       
       /* Make sure we got data of the right size */
       if (n != api_hdr.arg_len)
	 {
	   printf("Bogus lenght recieved from socket: %d\n",n);
	   close(sock);
	   return(-1);
	 }
     }

   processAPIreply(sock,&api_hdr,arg);
   return 0;
}

int testAPIsocket(int sock)
{
  struct apihdr api_hdr;
  struct qaoed_status status;
  int n;

   api_hdr.cmd = API_CMD_STATUS;  /* We want status info */
   api_hdr.type = REQUEST;  /* This is a request */
   api_hdr.error = API_ALLOK;  /* Everything is ok */
   api_hdr.arg_len = 0; /* no arguments */
   
   send(sock,&api_hdr,sizeof(api_hdr),0);

   /* Read api_hdr */
   n = recv(sock, &api_hdr, sizeof(struct apihdr),0);
   
   if(n != sizeof(struct apihdr))
     {
       printf("Status request failed\n");
       return(-1);
     }

   /* Read status */
   if(api_hdr.arg_len > 0)
     n = recv(sock, &status, sizeof(struct qaoed_status),0);

   if(api_hdr.cmd   != API_CMD_STATUS ||
      api_hdr.error != API_ALLOK ||
      api_hdr.type  != REPLY)
     {
       printf("Status request failed\n");
       return(-1);
     }

   return(0);
}

int openapi()
{
  int sock;
  struct sockaddr_un rsock;
  socklen_t sockaddr_len;

  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
     {
       fprintf(stdout,"Failed to create socket for API: socket() - %s\n",
	       strerror(errno));
       fflush(stdout);
       exit(1);
     }
      
   /* Setup struct */
   rsock.sun_family = AF_UNIX;
   strcpy(rsock.sun_path, SOCK_PATH);
   sockaddr_len = strlen(rsock.sun_path) + sizeof(rsock.sun_family) + 1;

   if (connect(sock, (struct sockaddr *)&rsock, sockaddr_len) == -1) 
     {
       fprintf(stdout,"Failed to connect to API: connect() - %s\n",
	       strerror(errno));
       fflush(stdout);
       exit(1);
     }

   if(testAPIsocket(sock) == -1)
     {
       close(sock);
       return(-1);
     }

   return(sock);

}

int add_target(int sock,char *device, int shelf, int slot, char *interface)
{
  struct apihdr api_hdr;
  struct qaoed_target_info target;
  
  /* api header */
  api_hdr.cmd     = API_CMD_TARGET_ADD;  /* We want to add a target */
  api_hdr.type    = REQUEST;              /* This is a request */
  api_hdr.error   = API_ALLOK;            /* Everything is ok */
  api_hdr.arg_len = sizeof(struct qaoed_target_info);
  
  /* Fill out target data */
  strcpy(target.devicename,device);
  target.shelf      = shelf;
  target.slot       = slot;
  if(interface != NULL)
    strcpy(target.ifname,interface);
  else
    target.ifname[0] = 0;
  target.writecache = -1;
  target.broadcast = -1;

  /* Send request */
  send(sock,&api_hdr,sizeof(api_hdr),0);  
  send(sock,&target,sizeof(target),0);  

  recvreply(sock);
  
  return(0);
}

int list_acl(int sock)
{
  struct apihdr api_hdr;

  /* api header */
  api_hdr.cmd     = API_CMD_ACL_LIST;  /* We want to add a target */
  api_hdr.type    = REQUEST;              /* This is a request */
  api_hdr.error   = API_ALLOK;            /* Everything is ok */
  api_hdr.arg_len = 0;

   /* Send request */
   send(sock,&api_hdr,sizeof(api_hdr),0);
   recvreply(sock);
   
   return(0);
}

int list_targets(int sock)
{
  struct apihdr api_hdr;

  /* api header */
  api_hdr.cmd     = API_CMD_TARGET_LIST;  /* We want to add a target */
  api_hdr.type    = REQUEST;              /* This is a request */
  api_hdr.error   = API_ALLOK;            /* Everything is ok */
  api_hdr.arg_len = 0;

   /* Send request */
   send(sock,&api_hdr,sizeof(api_hdr),0);
   recvreply(sock);
   
   return(0);
}




int main(int argc, char **argv)
{
   int sock;
   sock = openapi();
   
   list_acl(sock);
   //   add_target(sock, "/tmp/data",0xffff,0xff,NULL);
   list_targets(sock);
   
   close(sock);
   return(0);
}
