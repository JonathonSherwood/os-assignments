// JON SHERWOOD - CS 4760 - ASSIGNMENT 6 - MEMORY MANAGEMENT
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <wait.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <malloc.h>

#define SEMWAIT -1
#define SEMSIG 1
#define MAX_FRAMES 32	          // number of frames in system
#define MAX_MEM 256
#define NUM_PAGES 16           	  // number of pages available to processes
#define DEFAULT_PROCESS_COUNT 12  // default number of child processes
#define SEM_FLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define FILE_FLAGS (O_CREAT |O_RDWR |O_APPEND|O_SYNC)


typedef struct _page_data_t {
	int table_index;
	int page_num;
	int child_index;
	int range[2];     // memory range of page entry
	int dirty_bit;    // dirty bit is set if page modified or written into
	int reference;
	int valid;
	unsigned long reference_time;

} page_data_t;

typedef struct _page_table_t {
	page_data_t entries[MAX_FRAMES];
} page_table_t;


typedef struct _mem_reference_t {
	int rw;           // read write
	int address;
	pid_t pid;
	int child_index;
} mem_reference_t;

typedef struct _user_page_t {
	int page_num;
	int child_index;
	int range[2];
} user_page_t;

typedef struct {
	unsigned int sec;
	unsigned int nsec;
} logical_clock_t;

pid_t pids[DEFAULT_PROCESS_COUNT]; // pid arrray of users
int running;
int procs_in_system;
int num_page_faults;
int number_requests;
unsigned long long effective_access_time ;

// logging function
void writelog(char *msg) {
	time_t now;
	time(&now);
	char *timestr = (char *) malloc(12);
	strftime(timestr, 12, "%r", localtime(&now));
	char tmp[strlen(timestr) + strlen(msg) + 2];
	sprintf(tmp, "%s %s", timestr, msg);
	int fd = open("log", FILE_FLAGS, 0777);
	if (fd < 0) {
		perror("writelog: open failed");
		exit(EXIT_FAILURE);
	}
	// in case of any buffer issues
	write(fd, tmp, strlen(tmp));  // write to file
	close(fd);
}
