CC		= gcc
CFLAGS  = -g -Wall
TGTS	= oss slave
SRCS	= oss.c slave.c
LOG     = logfile
OBJS	= $(SRCS:.c=.o)

all:		$(TGTS) scrub
# Suffix rules 
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
scrub:
	rm -f *.o
clean:
	rm -f $(OBJS) $(TGTS) $(LOG) 
