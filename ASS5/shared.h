#ifndef SHARED_H
#define SHARED_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define MAX_CHILD 18
#define MAX_RESOURCES 20
#define INST_MAX 10
#define TIMEOUT 5
#define FILE_FLAGS (O_CREAT |O_RDWR |O_APPEND|O_SYNC)

// typedef enum { FALSE, TRUE } bool_t;

// resource descriptors
typedef struct {
	int resource_class_total;                // how many total instances of this resource
	int resource_class_available;            // how many available instances of this resource
	int shared;                              // resource shared indicator
	int max_claim[MAX_CHILD];
	int request[MAX_CHILD];
	int allocated[MAX_CHILD];
	int release[MAX_CHILD];                  // array for releases

} resource_t;


// data structure for shared memory
typedef struct {
	unsigned int sec;
	unsigned int nano_sec;
	unsigned int wait_nano;
	int run_flag[MAX_CHILD];                 // run status of fork'd children
	int throughput[MAX_CHILD];               // counter for child throughput
	int wait_time[MAX_CHILD];                // accumulator for child wait time
	unsigned long cpu_util[MAX_CHILD];       // accumulator for child cpu time
	resource_t resources[MAX_RESOURCES];     // array of resource descriptors
} shared_data_t;

// log write function
void writelog(char *msg) {
	time_t now;
	time(&now);
	char *timestr = (char *) malloc(12);
	strftime(timestr, 12, "%r", localtime(&now));
	char tmp[strlen(timestr) + strlen(msg) + 4];
	sprintf(tmp, "%s - %s", timestr, msg);
	int fd = open("log", FILE_FLAGS, 0777);
	if (fd < 0) {
		perror("writelog: open failed");
		exit(EXIT_FAILURE);
	}
	// using write in case of any buffer issue/race cond
	write(fd, tmp, strlen(tmp));    // write to file
	write (STDOUT_FILENO, msg, strlen (msg)); // write to terminal
	close(fd);
}

#endif
