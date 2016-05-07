CC		= gcc
CFLAGS  = -g -Wall
TGTS	= oss user
SRCS	= oss.c user.c
LOG     = log
OBJS	= $(SRCS:.c=.o)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:		$(TGTS)
clean:
	rm -f $(OBJS) $(TGTS) $(LOG)
