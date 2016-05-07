SHELL = /bin/sh
CC = gcc
CFLAGS = -g -Wall
TARGET = logger
TESTSRC = logtest.c
LIBSRC = liblogger.c
LIB = liblogger.so
.SUFFIXES: .c .o
$(TARGET): $(TESTSRC) $(LIB) 
	$(CC) $(CFLAGS) -o $@  $< -L. -llogger  -Wl,-rpath=.
$(LIB): $(LIBSRC)
	$(CC) $(CFLAGS) -shared -fPIC -o $@  $<
clean:
	/bin/rm -f *.o $(TARGET) $(LIB) 




########################################
## STATIC LIBRARY VERSION ################
########################################
#SHELL = /bin/sh
#CC = gcc
#CFLAGS = -Wall  -g
#TARGET = logger
#OBJS = liblogger.o
#AR = ar
#ARFLAGS = rvs
#LIBRARY = liblogger.a
#LIBLG = liblogger.c
#LOGTEST = logtest.c
#.SUFFIXES: .c .o .a
#$(TARGET):$(LOGTEST) $(LIBRARY)
#	$(CC) $(CFLAGS) -o $(TARGET) $(LOGTEST) $(LIBRARY)
#$(LIBRARY):$(OBJS)
#	$(AR) $(ARFLAGS) $@ $*.o
#.c.o:
#	$(CC) $(CFLAGS)  -c $(LIBLG) 
#clean:
#	/bin/rm -f *.o $(TARGET) $(LIBRARY) $(OBJS)



