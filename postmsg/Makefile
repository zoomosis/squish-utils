CDEFS=-DHAVE_TZSET
CFLAGS=-Wall -W -g
COPT=-O2

MSGAPI_INC=-I$(HOME)/opt/husky/include/smapi
MSGAPI_LIB=$(HOME)/opt/husky/lib/libsmapi.a

HUSKYLIB_INC=-I$(HOME)/opt/husky/include
HUSKYLIB_LIB=$(HOME)/opt/husky/lib/libhusky.a

SRCS=postmsg.c getopts.c llist.c prseaddr.c

postmsg: $(SRCS)
	$(CC) $(CDEFS) $(CFLAGS) $(COPT) $(MSGAPI_INC) $(HUSKYLIB_INC) -o postmsg $(SRCS) $(MSGAPI_LIB) $(HUSKYLIB_LIB)

clean:
	rm -f *.o postmsg
