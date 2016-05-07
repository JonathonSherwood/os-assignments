/* JON SHERWOOD: CS4760 - PROJECT 3 */
#ifndef _SHARED_H_
#define _SHARED_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <wait.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/sem.h>

#define DEFAULT_NUM_SLAVES 19
#define DEFAULT_MAX_PROC 20
#define DEFAULT_SLEEP_TIMEOUT 60
#define FILE_FLAGS (O_CREAT |O_RDWR |O_APPEND|O_SYNC)

typedef enum { FALSE, TRUE } bool_t;
typedef struct {
	int num_waiting_procs;         // # processes waiting on this condition
} cond_t;
typedef struct {
	int next_count;                // Number of processes waiting to enter the monitor
	int writing;
} ipcd_t;

char errmsg[50];            // For perror()
int num_children;           // Number of slaves

// Array of PIDs for slaves
pid_t slave_pid[DEFAULT_NUM_SLAVES];
cond_t  *notwriting;        // Condition variable
key_t SHMKEY1;              // Shared memory key
key_t SEMKEY1;              // Semaphore key 1
key_t SEMKEY2;              // Semaphore key 2
key_t SEMKEY3;              // Semaphore key 3
ipcd_t *ipcd;               // Pointer for shared memory
int shmid_ipcd;             // Shared memory ID
int semid_next;             // Semaphore ID for next
int semid_mutex;            // Semaphore ID for mutex
int semid_notwriting;       // Semaphore ID for notwriting

void monitor (char *);
void init();
void sem_init (int , int);
void sem_wait (int);
void sem_signal (int);
void cwait (cond_t *, int);
void csignal (cond_t *, int);
void cleanup (int);
char *current_dir();
void writelog (char *);

// return current directory for ftok
char *current_dir() {
	// get size of path
	long size = pathconf(".\n", _PC_PATH_MAX);
	char *buf;
	char *ptr;
	if ((buf = (char *) malloc((size_t) size)) != NULL) {
		if ((ptr = getcwd(buf, (size_t) size)) == NULL) {
			perror("oss: current_dir (getcwd)");
			exit(EXIT_FAILURE);
		}
	} else {
		perror("oss: current_dir (buf)");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

// write message to logfile "cstest"
void writelog (char *msg) {
	// get the time in string form
	time_t now;
	time (&now);
	char *timestr = (char *) malloc (6);
	strftime (timestr, 6, "%I:%M", localtime (&now));
	char tmp[52];
	sprintf (tmp, "File modified by process number %s at time %s\n", msg, timestr);

	if (ipcd->writing) {
		cwait (notwriting, semid_notwriting);
	}

	ipcd->writing = TRUE;    // Got past the wait(), raise the writing flag
	int fd = open ("cstest", FILE_FLAGS, 0777);

	if (fd < 0) {
		perror ("Open failed");
		exit (EXIT_FAILURE);
	}

	write (fd, tmp, strlen (tmp));
	write (STDOUT_FILENO, tmp, strlen (tmp));
	close (fd);
	ipcd->writing = FALSE;  // Done, lower the writing flag
	// Wake up next process waiting to write
	csignal (notwriting, semid_notwriting);
}

void monitor (char *line) {
	// Initialize monitor if not already done
	init ();
	// Lock the monitor
	sem_wait (semid_mutex);
	writelog (line);

	// If someone is waiting, wake him up, otherwise release the mutex
	if (ipcd->next_count > 0) {
		sem_signal (semid_next);
	} else {
		sem_signal (semid_mutex);
	}
}

void init () {
	// Generate a shared memory key
	if ( (SHMKEY1 = ftok (current_dir(), 'A')) == -1) {
		sprintf (errmsg, "Monitor: ftok (SHMKEY1)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Generate semaphore key 1
	if ( (SEMKEY1 = ftok (current_dir(), 'B')) == -1) {
		sprintf (errmsg, "Monitor: ftok (SEMKEY1)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Generate semaphore key 2
	if ( (SEMKEY2 = ftok (current_dir(), 'C')) == -1) {
		sprintf (errmsg, "Monitor: ftok (SEMKEY2)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Generate semaphore key 3
	if ( (SEMKEY3 = ftok (current_dir(), 'D')) == -1) {
		sprintf (errmsg, "Monitor: ftok (SEMKEY3)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Get a semaphore for "next"
	if ( (semid_next = semget (SEMKEY1, 1, IPC_CREAT | 0666)) == -1) {
		sprintf (errmsg, "Monitor: semget (next)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Get a semaphore for "mutex"
	if ( (semid_mutex = semget (SEMKEY2, 1, IPC_CREAT | 0666)) == -1) {
		sprintf (errmsg, "Monitor: semget (mutex)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Get a semaphore for "notwriting"
	if ( (semid_notwriting = semget (SEMKEY3, 1, IPC_CREAT | 0666)) == -1) {
		sprintf (errmsg, "Monitor: semget (notwriting)");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Initialize condition variable
	notwriting = malloc (sizeof (cond_t));
	notwriting->num_waiting_procs = 0;

	// Get the ID of the shared IPC data segment
	if ( (shmid_ipcd = shmget (SHMKEY1, sizeof (ipcd_t), IPC_CREAT | 0666)) == -1) {
		sprintf (errmsg, "Monitor: shmget");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	// Get a pointer to the shared IPC data segment
	if (! (ipcd = (ipcd_t *) (shmat (shmid_ipcd, 0, 0)))) {
		sprintf (errmsg, "Monitor: shmat");
		perror (errmsg);
		exit (EXIT_FAILURE);
	}

	ipcd->next_count = 0;
	ipcd->writing = 0;
	sem_init (semid_next, 0);
	sem_init (semid_mutex, 1);
	sem_init (semid_notwriting, 0);
}



// Semaphore initialization function
void sem_init (int semid, int semval) {
	// Define the semun union to be used as arg 4 of semctl
	union semun {
		int val;
		struct semid_ds *buf;
		ushort *array;
	} argument;
	// Set the semaphore value to one
	argument.val = semval;

	if (semctl (semid, 0, SETVAL, argument) == -1) {
		sprintf (errmsg, "Monitor: sem_init->semctl (semid: %d, value: %d)", semid,
				 semval);
		perror (errmsg);
		exit (EXIT_FAILURE);
	}
}


// Semaphore wait function
void sem_wait (int semid) {
	struct sembuf sbuf;        // Semaphore operation struct
	sbuf.sem_num = 0;          // First (and only) semaphore in set
	sbuf.sem_op = -1;          // Decrement the semaphore
	sbuf.sem_flg = 0;          // Operation flag

	if (semop (semid, &sbuf, 1) == -1)  {
		sprintf (errmsg, "Monitor: sem_wait->semop (semid: %d)", semid);
		perror (errmsg);
		exit (EXIT_FAILURE);
	}
}

// Semaphore signal function
void sem_signal (int semid) {
	struct sembuf sbuf;        // Semaphore operation struct
	sbuf.sem_num = 0;          // First (and only) semaphore in set
	sbuf.sem_op = 1;           // Increment the semaphore
	sbuf.sem_flg = 0;          // Operation flag

	if (semop (semid, &sbuf, 1) == -1)  {
		if (errno != EINTR) {
			sprintf (errmsg, "Monitor: sem_wait->semop (semid: %d)", semid);
			perror (errmsg);
			exit (EXIT_FAILURE);
		}
	}
}


void cwait (cond_t *this, int semid) {
	this->num_waiting_procs++;           // # processes waiting on this condition
	if (ipcd->next_count > 0) {          // Check for waiting process in monitor?
		sem_signal (semid_next);     // Wake up process
	} else {
		sem_signal (semid_mutex);    // No, free mutex so others can enter
	}
	sem_wait (semid);                    // Wait for condition
	this->num_waiting_procs--;           // End wait. Decriment process waiting
}

void csignal (cond_t *this, int semid) {
	if (this->num_waiting_procs <= 0) {     // No process waiting
		return;
	}

	ipcd->next_count++;        // # ready processes inside monitor
	sem_signal (semid);        // Send signal
	sem_wait (semid_next);     // Wait, let signalled process run
	ipcd->next_count--;        // Decriment processes in monitor
}


// Terminate all processes and free shared memory
void cleanup (int termsig) {
	int status;
	int i;

	// Terminate all slaves
	for (i = 1; i <= num_children; i++) {
		fprintf (stderr, "Attempting to terminate slave %d\n", i);
		kill (slave_pid[i], termsig);
		slave_pid[i] = wait (&status);
		fprintf (stderr, "Slave %d returned %d\n", i, WEXITSTATUS (status));
	}

	system ("./rmsem.sh");
	//exit (0);
}


#endif
