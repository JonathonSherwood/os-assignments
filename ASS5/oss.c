// JON SHERWOOD PROJECT 5
// CS 4760

#include "shared.h"

int signum;
int shmid_ipcd;
shared_data_t *shared_data_ptr;
int sem_id_clock;
int sem_id_resource;
char msgerr[50];
int child_pid[MAX_CHILD];
key_t shared_data_key;
key_t sem_key_clock;
key_t sem_key_resource;


char *current_dir();        // return current directory for ftok
int count_children();       // keep track of children
void usage();               // print progam usage
void writelog(char *);      // logfile writer

int term_proc(int , int);   // kill processes
void cleanup(int);          // clean up shared memory and semaphores
void sem_wait(int);
void sem_signal(int);
void fork_child(int);
pid_t r_wait(int *);
// Catch signals
void sigproc(int sig) {
	signum = sig;
}

// Process signals
int sigcheck() {
	signal(SIGINT, sigproc);
	signal(SIGTERM, sigproc);
	if (signum == 2 || signum == 15) {
		if (signum == 2) {
			fprintf(stderr, "oss:: Caught CTRL-C (SIGINT)\n");
		} else if (signum == 15) {
			printf("oss: Caught SIGTERM\n");
		}
		return 1;
	}
	return 0;
}


int term_proc(int child, int sig) {
	int status;  // Hold status from wait()
	sprintf(msgerr, "Attempting to terminate child %02d (PID %d)\n", child,
			child_pid[child]);
	writelog(msgerr);
	kill(child_pid[child], sig);
	//waitpid(child_pid[child], &status, 0);
	wait(&status);
	sprintf(msgerr, "Child %02d returned %d\n", child, WEXITSTATUS(status));
	writelog(msgerr);
	return WEXITSTATUS(status);
}

// Terminate all descendant processes and free shared memory
void cleanup(int termsig) {
	writelog("Executing cleanup()");
	// Terminate children
	int i;
	for (i = 0; i < MAX_CHILD; i++) {
		if (shared_data_ptr->run_flag[i] > 0) {
		if (term_proc(i, termsig) != 0) {
			sprintf(msgerr, "There was an issue terminating child %02d\n", i);
			fprintf(stderr, "oss:\t\t%s\n\n", msgerr);
			writelog(msgerr);
			}
		}
	}
	// Release shared memory
	int ipcd_ret = shmctl(shmid_ipcd, IPC_RMID, (struct shmid_ds *) NULL);
	if (ipcd_ret != 0) {
		fprintf(stderr, "Error releasing shared memory - please clear manually\n");
		system("./clean.sh");
	} else {
		writelog("Released shared memory successfully");
	}
	// Remove clock semaphore
	if ((semctl(sem_id_clock, 0, IPC_RMID, 1) == -1) && (errno != EINTR)) {
		sprintf(msgerr, "oss: cleanup->semctl (clock)");
		perror(msgerr);
	} else {
		writelog("Removed clock semaphore successfully");
	}
	// Remove resource semaphore
	if ((semctl(sem_id_resource, 0, IPC_RMID, 1) == -1) && (errno != EINTR)) {
		sprintf(msgerr, "oss: cleanup->semctl (resources)");
		perror(msgerr);
	} else {
		writelog("Removed resource semaphore successfully");
	}
	system("./clean.sh");
}


int main(int argc, char **argv) {
	int child_sel;
	int i, x;                                    // iteration variables
	int num_shared;                              // number of shared resources
	int need;                                    // Need variable for deadlock detection calculations
	unsigned int tot_tp = 0, tot_wt = 0;
	unsigned long tot_cu = 0;
	float avg_tp, avg_tt, avg_wt, avg_cu;
	srand(time(NULL));
	// Signal initializations
	if (signal(SIGINT, sigproc) == SIG_ERR) {
		fprintf(stderr, "can't catch SIGINT\n");
		exit(EXIT_FAILURE);
	}
	// gen shared memory key
	if ((shared_data_key = ftok(current_dir(), 'A')) == -1) {
		perror("oss: ftok shared_data_key");
		writelog("oss: ftok shared_data_key");
		exit(1);
	}
	// get semaphore key for clock
	if ((sem_key_clock = ftok(current_dir(), 'B')) == -1) {
		perror("oss: ftok sem_key_clock");
		writelog("oss: ftok sem_key_clock");
		exit(1);
	}
	// get semaphore key for resource
	if ((sem_key_resource = ftok(current_dir(), 'C')) == -1) {
		perror("oss: ftok sem_key_resource");
		writelog("oss: ftok sem_key_resource");
		exit(1);
	}
	// get memory for the shared data
	if ((shmid_ipcd = shmget(shared_data_key, sizeof(shared_data_t) ,
							 0600 | IPC_CREAT)) == -1) {
		perror("oss: shmget");
		writelog("oss: shmget");
		exit(1);
	}
	// get pointer to the shared data
	if ((shared_data_ptr = shmat(shmid_ipcd, 0, 0)) == (void *) - 1) {
		// if (! (shared_data_ptr = (shared_data_t *) (shmat (shmid_ipcd, 0, 0)))) {
		sprintf(msgerr, "oss: shmat");
		perror(msgerr);
		exit(1);
	}
	// get a semaphore for the resources
	if ((sem_id_resource = semget(sem_key_resource, 1, 0600 | IPC_CREAT)) == -1) {
		sprintf(msgerr, "oss: semget (resources)");
		perror(msgerr);
		exit(1);
	}
	// get a semaphore for the logical clock
	if ((sem_id_clock = semget(sem_key_clock, 1, 0600 | IPC_CREAT)) == -1) {
		perror("oss: semget ");
		writelog("oss: semget ");
		exit(1);
	}
	// init resource semaphore
	if (semctl(sem_id_resource, 0, SETVAL, 1) == -1) {
		sprintf(msgerr, "oss: semctl (resources)");
		perror(msgerr);
		exit(1);
	}
	// init the clock semaphore
	if (semctl(sem_id_clock, 0, SETVAL, 1) == -1) {
		sprintf(msgerr, "oss: semctl (clock)");
		perror(msgerr);
		exit(1);
	}
	// wait on clock semaphore
	sem_wait(sem_id_clock);
	// init the logical clock
	shared_data_ptr->sec = 0;
	shared_data_ptr->nano_sec = 0;
	shared_data_ptr->wait_nano = 0;     // SEM_UNDO
	sem_signal(sem_id_clock);           // signal on clock semaphore
	sprintf(msgerr, "Initialized clock");
	writelog(msgerr);
	// RANDOM SHARED RESOURCES [10% - 25%]
	num_shared = MAX_RESOURCES * ((float)(15 + (rand() % 10)) / 100);
	sprintf(msgerr, "Determined that %d of %d resources will be shared\n",
			num_shared, MAX_RESOURCES);
	writelog(msgerr);
	// initialize the resources
	for (i = 0; i < MAX_RESOURCES; i++) {
		// Set total number of instances for this resource
		shared_data_ptr->resources[i].resource_class_total = 1 + (rand() % INST_MAX);
		shared_data_ptr->resources[i].resource_class_available =
			shared_data_ptr->resources[i].resource_class_total;
		sprintf(msgerr, "Resource %d will have %d instances\n", i,
				shared_data_ptr->resources[i].resource_class_total);
		writelog(msgerr);
		// Set the shared flag for this resource
		if (i <= num_shared) {
			shared_data_ptr->resources[i].shared = 1;
			sprintf(msgerr, "Resource %d will be shared\n", i);
			writelog(msgerr);
		} else {
			shared_data_ptr->resources[i].shared = 0;
		}
		// init max_claim, request, allocated, and release arrays
		for (x = 0; x < MAX_CHILD; x++) {
			shared_data_ptr->resources[i].max_claim[x] = 0;
			shared_data_ptr->resources[i].request[x]   = 0;
			shared_data_ptr->resources[i].allocated[x] = 0;
			shared_data_ptr->resources[i].release[x]   = 0;
		}
	}
	// init child runninng array
	for (i = 0; i < MAX_CHILD; i++) {
		shared_data_ptr->run_flag[i] = 0;
	}
	while (1) {
		// Check for signals
		if (sigcheck()) {
			break;
		}
		/* resources allocation      */
		// determine who has released resources and update the resource data
		for (i = 0; i < MAX_RESOURCES; i++) {
			for (x = 0; x < MAX_CHILD; x++) {
				if (shared_data_ptr->resources[i].release[x] != 0) {
					sprintf(msgerr, "Child %2d has released %d\n", x,
							shared_data_ptr->resources[i].release[x]);
					sprintf(msgerr, "%s instances of resource %d\n", msgerr, i);
					writelog(msgerr);
					shared_data_ptr->resources[i].resource_class_available +=
						shared_data_ptr->resources[i].release[x];
					// Wait on semaphore, update values, signal on semaphore
					sem_wait(sem_id_resource);
					shared_data_ptr->resources[i].release[x] = 0;
					sem_signal(sem_id_resource);
				}
			}
		}
		// Allocate resources based on updated allocation and new requests
		for (i = 0; i < MAX_RESOURCES; i++) {
			for (x = 0; x < MAX_CHILD; x++) {
				// PROCESS REQUESTS
				if (shared_data_ptr->resources[i].request[x] > 0) {
					// need[i][j] =  max[i][j] − allocation[i][j].
					need = shared_data_ptr->resources[i].max_claim[x] -
						   shared_data_ptr->resources[i].allocated[x];
					// check request < max allowed
					if (shared_data_ptr->resources[i].request[x] > need ||
							shared_data_ptr->resources[i].allocated[x] +
							shared_data_ptr->resources[i].request[x] >
							shared_data_ptr->resources[i].resource_class_total) {
						sprintf(msgerr, "child ^%d req > maximum \n", x);
						writelog(msgerr);
						//
						sem_wait(sem_id_resource);
						shared_data_ptr->resources[i].request[x] = -1;
						sem_signal(sem_id_resource);
					} else {
						// Perform deadlock detection
						if (shared_data_ptr->resources[i].request[x] <=
								shared_data_ptr->resources[i].resource_class_available) {
							// Update throughput statistic for this process
							shared_data_ptr->throughput[x] += shared_data_ptr->resources[i].request[x];
							// Allocate the resource
							sprintf(msgerr, "Child %2d has allocated %d\n", x,
									shared_data_ptr->resources[i].request[x]);
							sprintf(msgerr, "%s of resource %d\n", msgerr, i);
							writelog(msgerr);
							shared_data_ptr->resources[i].resource_class_available -=
								shared_data_ptr->resources[i].request[x];
							shared_data_ptr->resources[i].allocated[x] +=
								shared_data_ptr->resources[i].request[x];
							sem_wait(sem_id_resource);
							shared_data_ptr->resources[i].request[x] = 0;
							sem_signal(sem_id_resource);
						}
					}
				}
			}
		}
		/*
		    FORK AND EXEC CHILDREN
		*/
		// Only attempt a fork another child if we are below MAX_CHILD processes
		if (count_children() < MAX_CHILD) {
			for (i = 0; i <= MAX_CHILD; i++) {
				if (shared_data_ptr->run_flag[i] == 0) {
					child_sel = i;
					break;
				}
			}
			sprintf(msgerr, "Selected child number %d to fork\n", child_sel);
			writelog(msgerr);
			fork_child(child_sel);
		}
		sem_wait(sem_id_clock); // Wait for the clock to become available
		// fork user process at random times 1 - 500ms [1000 nanoseconds = 1 ms ]
		shared_data_ptr->wait_nano += (1000 + (rand() % 500000000));
		//  advance the clock
		if (shared_data_ptr->nano_sec >= 1000000) {
			shared_data_ptr->sec++;
			shared_data_ptr->nano_sec -= 1000000;
		}
		// Signal that the clock is now available
		sem_signal(sem_id_clock);
		sprintf(msgerr, "Logical clock is now %u.%u\n", shared_data_ptr->sec,
				shared_data_ptr->nano_sec);
		writelog(msgerr);
		/* PRINT STATISTICS */
		for (i = 0; i < MAX_CHILD; i++) {
			tot_tp += shared_data_ptr->throughput[i];       // total throughput
			tot_wt += shared_data_ptr->wait_time[i];        // total wait time
			tot_cu += shared_data_ptr->cpu_util[i];         // total CPU utilization
			//fprintf(stderr,("tot_cu = %lu\n\n", tot_cu);
		}
		if (MAX_CHILD > 0) {                     // Handle possibility of divide by zero
			avg_tp = (float)tot_tp / MAX_CHILD;  // average throughput
			// average turnaround time
			if (tot_tp > 0) {
				avg_tt = ((float)tot_wt / tot_tp) / MAX_CHILD;
			}
			avg_wt = (float)tot_wt / MAX_CHILD;  // average wait time
			avg_cu = ((float)tot_cu / MAX_CHILD) / 1000000;  // average cpu time
		}

		fprintf(stderr, "\e[1;1H\e[2J");   // Clear the screen
		fprintf(stderr, "Statistics after %u.%u seconds\n\n", shared_data_ptr->sec,
				shared_data_ptr->nano_sec);
		fprintf(stderr, "Average Throughput:\t\t%.2f  allocations per process\n\n",
				avg_tp);
		fprintf(stderr, "Average Turnaround Time:\t%.2f  milliseconds\n\n", avg_tt);
		fprintf(stderr, "Average Wait Time:\t\t%.2f  milliseconds\n\n", avg_wt);
		fprintf(stderr, "Average CPU Utilization:\t%.2f  milliseconds\n\n", avg_cu);
	}
	// cleanup [just in case]
/*
	if (signum) {
		cleanup(signum);     // Call cleanup with whatever the signal was. We should not
	} else {
		cleanup(SIGTERM);     //  ever get here without receiving a signal, but just in
	}
*/


	system("./clean.sh");

	return 0;
}

void usage() {
	fprintf(stderr, "USAGE: master [options] \n\n");
}


// Semaphore wait function
void sem_wait(int semid) {
	struct sembuf sbuf;  // Semaphore operation struct
	sbuf.sem_num = 0;    // only
	sbuf.sem_op = -1;    // Decrement the semaphore
	sbuf.sem_flg = 0;    // Operation flag
	semop(semid, &sbuf, 1);
}

// Semaphore signal function
void sem_signal(int semid) {
	struct sembuf sbuf;  // Semaphore operation struct
	sbuf.sem_num = 0;    // First (and only) semaphore in set
	sbuf.sem_op  = 1;    // Increment the semaphore
	sbuf.sem_flg = SEM_UNDO;    // Operation flag
	semop(semid, &sbuf, 1);
}

int count_children() {
	int i, count = 0;
	for (i = 0; i < MAX_CHILD; i++) {
		// Check for child running status. If not, attempt to clear the process. If so, increment count.
		if (shared_data_ptr->run_flag[i] == 0 && child_pid[i] > 0) {
			sprintf(msgerr, "Child %2d has exited - attempting to clean it up\n", i);
			writelog(msgerr);
			if (term_proc(i, SIGTERM) != 0) {
				sprintf(msgerr, "There was an issue terminating child %2d\n", i);
				writelog(msgerr);
				perror(msgerr);
				cleanup(SIGTERM);
				exit(1);
			}
			child_pid[i] = 0;
		}
		// If this child is running, increment the total children count
		if (shared_data_ptr->run_flag[i]) {
			count++;
		}
	}
	sprintf(msgerr, "Current child count is %d\n", count);
	writelog(msgerr);
	return count;
}

// Child forking function
void fork_child(int child) {
	char child_arg[3];
	if ((child_pid[child] = fork()) < 0) {
		sprintf(msgerr, "oss: fork() for child %2d\n", child);
		perror(msgerr);
		writelog("Error forking child");
		cleanup(SIGTERM);
		exit(1);
	} else {
		if (child_pid[child] == 0) {
			// exec child
			sprintf(child_arg, "%2d", child);
			execl("./user", "user", child_arg, (char *)NULL);
			// Handle execl() error, if one occurs
			sprintf(msgerr, "oss: exec child %2d after fork\n", child);
			perror(msgerr);
			writelog(msgerr);
		} else {
			// This is the parent; write to oss log about fork()
			sprintf(msgerr, "Forked process ID %d for child %2d\n", child_pid[child],
					child);
			writelog(msgerr);
			// Set running status of child
			sprintf(msgerr, "Setting child %2d status 'running'\n", child);
			writelog(msgerr);
			shared_data_ptr->run_flag[child] = 1;
		}
	}
}

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

// PID array for child processes
pid_t r_wait(int *stat_loc) {
	int retval;
	while (((retval = wait(stat_loc)) == -1) && (errno == EINTR));
	return retval;
}
