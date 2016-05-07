CC            = gcc
CFLAGS        = -g -Wall  -pthread  
TGTS          = oss user
SRCS          = oss.c user.c
OBJS          = $(SRCS:.c=.o)
LOG			  = log
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
all:	$(TGTS) 
clean:
	rm $(OBJS) $(TGTS) $(LOG) 
