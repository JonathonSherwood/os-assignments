
SHELL = /bin/sh
CC = gcc
CFLAGS = -g -Wall
MASTER = master
OBJMASTER = master.o
SLAVE = slave 
OBJSLAVE = slave.o
LOG = cstest
.SUFFIXES: .c .o
all: $(MASTER) $(SLAVE)

$(MASTER): $(OBJMASTER)
	$(CC) $(CFLAGS) -o $@ $^
$(SLAVE): $(OBJSLAVE)
	$(CC) $(CFLAGS) -o $@ $^
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
clean:
	rm -f $(MASTER) $(SLAVE) *.o $(LOG)


