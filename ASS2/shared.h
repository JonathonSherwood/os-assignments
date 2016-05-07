#ifndef _SHARED_H_
#define _SHARED_H_

#define MAX_WRITES 3

enum state { idle, want_in, in_cs }; // Function to display usage information
typedef struct {
	int shared_mem_flag[19];
	int turn;
} shared_mem_flag;


int signal_id;

void usage() {
	printf ("USAGE: master [options] \n\n");
	printf ("OPTIONS:\n");
	printf ("\t-h  help \n");
	printf ("\t-n  [# of children 1 to 19] default is 19 \n");
	printf ("\t-t  [# of secs for child timeout] default is 60 \n");
}

#endif
