
/* API command codes */
enum {
     API_CMD_STATUS,
     API_CMD_INTERFACES,
     API_CMD_INTERFACES_STATUS,
     API_CMD_INTERFACES_ADD,
     API_CMD_INTERFACES_DEL,
     API_CMD_INTERFACES_SETMTU,
     API_CMD_TARGETS,
     API_CMD_TARGETS_STATUS,
     API_CMD_TARGETS_ADD,
     API_CMD_TARGETS_DEL,
     API_CMD_TARGETS_SETOPTION,
     API_CMD_TARGETS_SETACL,
     API_CMD_ACL,
     API_CMD_ACL_STATUS,
     API_CMD_ACL_ADD,
     API_CMD_ACL_DEL
};

enum {
  REQUEST,
  REPLY
};

enum {
  API_ALLOK,
  API_UKNOWNCMD,
  API_FAILURE
};

/* Header used in all requests and replies */
struct apihdr 
{
   unsigned char cmd; /* Command */
   unsigned char type; /* Request == 0, Reply == 1; */
   unsigned char error; /* OK == 0 */
   unsigned int  arg_len; /* Lenght of additional argument buffer */
};

/* qaoed status struct, describes current status of qaoed */
struct qaoed_status 
{
   unsigned int num_targets; /* Number of targets */
   unsigned int num_interfaces; /* Number of interfaces */
   unsigned int num_threads; /* Number of running threads */
};

/* qaoed status struct, describes current status of qaoed */
struct qaoed_interfaces
{
   unsigned int num_interfaces; /* Number of targets */
   unsigned int refcounter; /* Number of users of this interfaces */
};


/* Used to adding / removing interfaces */
struct qaoed_interface_cmd
{
   char ifname[20];        /* Name of interface */
   int mtu;                /* the Maximum Transfer Unit (MTU) of this int... */
};

/* Used to request status and to add / remove targets */
struct qaoed_target_cmd
{
   char devicename[120];       /*  file/device name */
   unsigned short shelf;   /* 16 bit */
   unsigned char slot;     /*  8 bit */
   char ifname[20];        /* Name of interface */
   int writecache;         /* Writecache on or off */
   int broadcast;          /* Broadcast on or off */
};


