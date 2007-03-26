CC=gcc
CFLAGS= -Wall -g -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
LIBS = -lpthread

all:    qaoed

OBJS  = devices.o network.o main.o logging.o acl.o parseconf.o
RCFGOBJS = rcfg/readconf.o
ARCHOBJS = arch/bsd_net.o arch/linux_net.o arch/bsd_blk.o arch/linux_blk.o
INCLUDES = include/acl.h include/eth.h include/aoe.h include/qaoed.h \
	   include/hdreg.h include/logging.h include/byteorder.h

$(OBJS): $(INCLUDES)

clean:
	rm -f *.o *~ core qaoed include/*~ rcfg/*~ rcfg/*.o arch/*.o \
	arch/*~ man/*~ examples/*~

qaoed:	$(OBJS) $(RCFGOBJS) $(ARCHOBJS) $(INCLUDES)
	make -C arch
	make -C rcfg
	gcc $(CFLAGS) -o qaoed $(OBJS) $(RCFGOBJS) $(ARCHOBJS) $(LIBS)
	strip qaoed 



