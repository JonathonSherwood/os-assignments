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
#define CHILD_MAX 18
#define GLOBAL_TIMEOUT 2
#define FILE_FLAGS (O_CREAT |O_RDWR |O_APPEND|O_SYNC)

typedef struct{
	unsigned int sec;
	unsigned int nano_sec;
} shared_clock_t;

// process control block
typedef struct {
	// clear pcb flag
	int clear_pcb;
	unsigned long long previous_burst;
	// time totals
	unsigned long long sys_time;
	unsigned long long cpu_time;
	unsigned long long wait_time;
	pid_t pid;
} pcb_t;


typedef struct {
	unsigned int quantum;
	pcb_t pcb[CHILD_MAX];
	shared_clock_t shared_clock;
	int sem;
	int turn;
} shared_data;

char msgstr[50];
//shared_data *d;
int l;
void sem_wait(int, int);
void sem_signal(int, int);


void sem_wait(int semid, int index) {

	struct sembuf sembuff;
	sembuff.sem_num = index;
	sembuff.sem_op = -1;
	sembuff.sem_flg = SEM_UNDO;
	semop(semid, &sembuff, 1);

}

void sem_signal(int semid, int index) {
	struct sembuf sembuff;
	sembuff.sem_num = index;
	sembuff.sem_op = 1;
	sembuff.sem_flg = SEM_UNDO;
	semop(semid, &sembuff, 1);

}
char *curTime() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char *currentTime = malloc(9);
	sprintf(currentTime, "%d:%d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	return currentTime;
}

void writelog (char *msg) {
    time_t now;
	time (&now);
	char *timestr = (char *) malloc (12);
    strftime (timestr, 12, "%r", localtime (&now));
    char tmp[strlen(timestr)+strlen(msg)+2];
    sprintf (tmp,"%s %s\n",timestr,msg);
    int fd = open ("logfile", FILE_FLAGS, 0777);
	if (fd < 0) {
		perror ("writelog: open failed");
		exit (EXIT_FAILURE);
	}
    // in case of any buffer issues
	write (fd, tmp, strlen (tmp));            // write to file
	// write (STDOUT_FILENO, msg, strlen (msg)); // write to terminal
	close (fd);
}




#endif
