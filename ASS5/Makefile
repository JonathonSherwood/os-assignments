CFLAGS	= -g -Wall
TGTS	= oss user
SRCS	= oss.c user.c
OBJS	= $(SRCS:.c=.o)
LOG     = log
all:		$(TGTS)
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
clean:
	rm -f $(OBJS) $(TGTS) $(LOG)
