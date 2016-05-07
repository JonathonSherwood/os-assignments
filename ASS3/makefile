
#SHELL = /bin/sh
#CC = gcc
#CFLAGS = -g -Wall
#MASTER = master
#OBJMASTER = master.o
#SLAVE = slave 
#OBJSLAVE = slave.o
#LOG = cstest
#.SUFFIXES: .c .o
#all: $(MASTER) $(SLAVE)

#$(MASTER): $(OBJMASTER)
#	$(CC) $(CFLAGS) -o $@ $^
#$(SLAVE): $(OBJSLAVE)
#	$(CC) $(CFLAGS) -o $@ $^
#.c.o:
#	$(CC) $(CFLAGS) -c -o $@ $<
#clean:
#	rm -f $(MASTER) $(SLAVE) *.o $(LOG)

CC            = gcc
CFLAGS        = -g -Wall
TGTS          = oss slave
SRCS          = oss.c slave.c
OBJS          = $(SRCS:.c=.o)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:	$(TGTS) 
# oss:          main.o $(COMMOBJ)
# 	$(CC) -o $@ $^
#
# userproc:     userproc.o $(COMMOBJ)
# 	$(CC) -o $@ $^
clean:
	rm $(OBJS) $(TGTS) *log

