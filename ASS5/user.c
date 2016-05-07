// JON SHERWOOD PROJECT 5
// CS 4760

#include "shared.h"

int shmid_ipcd;                                  // shared memory id
int signum;                                      // hold a signal number
char msgerr[50];
shared_data_t *shared_data_ptr;                  // pointer for shared memory
int sem_id_clock;                                // clock semaphore id
int sem_id_resource;                             // resource semaphore id
int child_id_number;
int child_pid[MAX_CHILD];

pid_t r_wait(int *stat_loc);
char *current_dir();
void sig_handler(int);
void sem_wait(int );
void sem_signal(int);

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Process number was not passed in call to user\n");
		exit(1);
	} else {
		child_id_number = atoi(argv[1]);
	}
	// Local variables
	key_t shared_data_key;
	key_t sem_key_clock;
	key_t sem_key_resource;

	int n;                                       // iteration variable
	int release, request;                        // request/release variables
	unsigned int start_sec;                      // termination timer
	srand(time(NULL));
	child_id_number = getpid();                            // current process ID
	sprintf(msgerr, "Starting new run with PID %d", child_id_number);
	writelog(msgerr);

	/* KEYS */
	// gen shared memory key
	if ((shared_data_key = ftok(current_dir(), 'A')) == -1) {
		sprintf(msgerr, "user %2d: ftok (shared_data_key)", child_id_number);
		perror(msgerr);
		writelog(msgerr);
		exit(1);
	}
	// get semaphore key for clock
	if ((sem_key_clock = ftok(current_dir(), 'B')) == -1) {
		sprintf(msgerr, "user %2d: ftok (sem_key_clock)", child_id_number);
		perror(msgerr);
		writelog(msgerr);
		exit(1);
	}
	// get semaphore key for resource
	if ((sem_key_resource = ftok(current_dir(), 'C')) == -1) {
		sprintf(msgerr, "user %2d: ftok (sem_key_resouce)", child_id_number);
		perror(msgerr);
		writelog(msgerr);
		exit(1);
	}
	// Get the ID of the shared IPC data
	if ((shmid_ipcd = shmget(shared_data_key, sizeof(shared_data_t) ,
							 0600)) == -1) {
		sprintf(msgerr, "user %2d: shmget", child_id_number);
		perror(msgerr);
		exit(1);
	}
	// Get a pointer to the shared IPC data segment
	if (!(shared_data_ptr = (shared_data_t *)(shmat(shmid_ipcd, 0, 0)))) {
		sprintf(msgerr, "user %2d: shmat", child_id_number);
		perror(msgerr);
		exit(1);
	}
	writelog("Attached to shared memory for IPC data");
	// Get a semaphore for the logical clock
	if ((sem_id_clock = semget(sem_key_clock, 1, 0600)) == -1) {
		sprintf(msgerr, "user %2d: semget (clock)", child_id_number);
		perror(msgerr);
		exit(1);
	}
	// Get a semaphore for resources
	if ((sem_id_resource = semget(sem_key_resource, 1, 0600)) == -1) {
		sprintf(msgerr, "user %2d: semget (resource)", child_id_number);
		perror(msgerr);
		exit(1);
	}
	writelog("Attached to clock and resource semaphores");
	// Determine which resources we will use, and maximum claim for each
	int res_use;                                 // Use this resource or not
	for (n = 0; n < MAX_RESOURCES; n++) {
		res_use = rand() % 4;                    // Set 0-3 randomly (we will use on 1)
		if (res_use == 1) {
			// Enter our max claim for this resource (random 1 - total number of instances)
			shared_data_ptr->resources[n].max_claim[child_id_number] = 1 + (rand() %
					shared_data_ptr->resources[n].resource_class_total);
			sprintf(msgerr, "Using a maximum of %d of resource %d",
					shared_data_ptr->resources[n].max_claim[child_id_number], n);
			writelog(msgerr);
		}
	}

	// initialize statistics
	shared_data_ptr->throughput[child_id_number] = 0;
	shared_data_ptr->wait_time[child_id_number] = 0;
	shared_data_ptr->cpu_util[child_id_number] = 0;
	// start the termination timer
	start_sec = shared_data_ptr->sec;
	// at random times (between 0 and 250ms), the process checks if it should terminate.
	// if so, it should deallocate all the resources
	// shared_data_ptr->wait_nano = rand() % 250000000; //250ms
	while (1) {
		if ((rand() % 10) == 1 && shared_data_ptr->sec - start_sec > 0) {
			// Terminate myself
			for (n = 0; n < MAX_RESOURCES; n++) {
				shared_data_ptr->resources[n].request[child_id_number] = 0;
				shared_data_ptr->resources[n].release[child_id_number] = 0;
				shared_data_ptr->resources[n].allocated[child_id_number] = 0;
			}
			shared_data_ptr->run_flag[child_id_number] = 0;
			writelog("***** EXITING *****\n\n");
			exit(0);
		}
		/*
		    RESOURCE PROCESSING
		*/
		// Randomly request (if there is not already a request in the pipe) or release (only
		//   if we have some resources allocated) resources
		for (n = 0; n < MAX_RESOURCES; n++) {
			// Decide whether to release
			if (shared_data_ptr->resources[n].allocated[child_id_number] > 0 && rand() % 2 == 1) {
				if (rand() % 2 == 1) {
					// Enter a release for this resource (random 0 - allocated), add to current release
					release = rand() % shared_data_ptr->resources[n].allocated[child_id_number];
					sem_wait(sem_id_resource);         // wait on resource semaphore
					shared_data_ptr->resources[n].release[child_id_number] += release;
					sem_signal(sem_id_resource);       // signal on resource semaphore
					sprintf(msgerr, "Released %d instances of resource %d", release, n);
					writelog(msgerr);
					// Update CPU utilization
					shared_data_ptr->cpu_util[child_id_number] += 10; // Add 10 nanoseconds
				}
			}
			// Or request
			else if (shared_data_ptr->resources[n].request[child_id_number] == 0) {
				if (shared_data_ptr->resources[n].max_claim[child_id_number] > 0 && rand() % 2 == 1) {
					// Enter a request for this resource (random 0 - (max - allocated))
					request = rand() % (shared_data_ptr->resources[n].max_claim[child_id_number] -
										shared_data_ptr->resources[n].allocated[child_id_number]);
					if (request > 0) {           // If request is 0, don't bother
						sem_wait(sem_id_resource);     // wait on resource semaphore
						shared_data_ptr->resources[n].request[child_id_number] = request;
						sem_signal(sem_id_resource);   // signal on resource semaphore
						sprintf(msgerr, "Requested %d instances of resource %d", request, n);
						writelog(msgerr);
						// Update CPU utilization
						shared_data_ptr->cpu_util[child_id_number] += 15000000; // Add 15 milliseconds
					}
				}
			}
		}
		/*
		    CLOCK UPDATE
		*/
		// Signal that the clock is now available
		sem_signal(sem_id_clock);                   // signal on clock semaphore
		shared_data_ptr->wait_nano += (1 + (rand() %
											250000000)); // user random check [0 - 250ms]
		// shared_data_ptr->nano_sec += rand() % 500000; //50ms
		if (shared_data_ptr->nano_sec >= 1000000) {
			shared_data_ptr->sec++;
			// check if processes should terminate
			int shouldTerminate = rand() % 2;\
		}
		// Signal that the clock is now available
		sem_signal(sem_id_clock);
		sprintf(msgerr, "Logical clock is now %u.%u", shared_data_ptr->sec,
				shared_data_ptr->nano_sec);
		writelog(msgerr);

		// Update the wait and cpu statistics
		shared_data_ptr->wait_time[child_id_number] += rand() % 1;
		// print cpu utilization
		fprintf(stderr, "user %2d: CPU util is %ld nanoseconds\n", child_id_number,
				shared_data_ptr->cpu_util[child_id_number]);

	}
	return 0;
}


pid_t r_wait(int *stat_loc) {
	int retval;
	while (((retval = wait(stat_loc)) == -1) && (errno == EINTR));
	return retval;
}
// return current directory for ftok
char *current_dir() {
	// get size of path
	long size = pathconf(".", _PC_PATH_MAX);
	char *buf;
	char *ptr;
	if ((buf = (char *) malloc((size_t) size)) != NULL) {
		if ((ptr = getcwd(buf, (size_t) size)) == NULL) {
			perror("user: current_dir (getcwd)");
			exit(EXIT_FAILURE);
		}
	} else {
		perror("user: current_dir (buf)");
		exit(EXIT_FAILURE);
	}
	return ptr;
}
void sig_handler(int signo) {
	if (signo == SIGINT) {
		sprintf(msgerr, "User Process %d caught SIGINT - Exiting.\n", getpid());
		writelog(msgerr);
		exit(0);
	} else if (signo == SIGALRM) {
		sprintf(msgerr, "User Process %d - Killed.\n", getpid());
		writelog(msgerr);
		exit(0);
	}
}// semaphore wait function
void sem_wait(int semid) {
	struct sembuf sbuf;
    sbuf.sem_num = 0;
    // negative sem_op, we want to obtain
    // resources that the semaphore controls.
	sbuf.sem_op = -1;
    sbuf.sem_flg = SEM_UNDO;
    semop(semid, &sbuf, 1);
}

// semaphore signal function
void sem_signal(int semid) {
	struct sembuf sbuf;
    sbuf.sem_num = 0;
    // sem_op is positive. so return
    // resources specified by the process.
	sbuf.sem_op  = 1;
	sbuf.sem_flg = SEM_UNDO;
	semop(semid, &sbuf, 1);
}
