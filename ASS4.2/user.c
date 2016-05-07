
#include "shared.h"

void signal_handler() ;
void signal_init() ;


int main(int argc, char **argv) {
	int semid_oss;
	int shmid_shared;
	int process_pid;
	int process_index;
	char msg[50];
	shared_datat_t *shared;
	process_index = atoi(argv[1]);
	key_t SHMKEY;
	key_t SEMKEY;
	process_pid = getpid();    // current process pid
	sprintf(msg, "Starting mew process PID %d\n", process_pid);
	writelog(msg);
	write(STDOUT_FILENO, msg, strlen(msg));
	int if_use_full_quantum;
	unsigned int quantum_used; // amount of quantum  used
	srand(time(NULL));
	signal_init();

	/* SHARED MEMORY */
	// shared memory key
	if ((SHMKEY =  ftok("./oss.c", 'A')) == -1) {
		perror("ftok SHMKEY");
		exit(EXIT_FAILURE);
	}

	// semaphore key
	if ((SEMKEY = ftok("./oss.c", 'B')) == -1) {
		perror("ftok SEMKEY");
		exit(EXIT_FAILURE);
	}

	// Get the ID of the shared IPC data
	if ((shmid_shared = shmget(SHMKEY, sizeof(shared_datat_t), 0600)) == -1) {
		sprintf(msg, "process %02d: shmget\n", process_index);
		perror(msg);
		exit(EXIT_FAILURE);
	}

	if (!(shared = (shared_datat_t *)(shmat(shmid_shared, 0, 0)))) {
		sprintf(msg, "process %02d: shmat\n", process_index);
		perror(msg);
		exit(EXIT_FAILURE);
	}

	// Get a semaphore for signalling between oss and user
	if ((semid_oss = semget(SEMKEY, 1, 0600)) == -1) {
		sprintf(msg, "process %02d: semget\n", process_index);
		perror(msg);
		exit(EXIT_FAILURE);
	}

	while (1) {
		// check if current process pid is scheduled
		if (shared->selected_process_pid_to_sched == process_pid && shared->child_pcb[process_index].run_status == 0) {
			// set last_run_time
			shared->child_pcb[process_index].last_run_time = (float) shared->sec + ((float) shared->n_sec / 1000);
			sprintf(msg, "process %02d: last run time set to: %8.2f\n", process_index, shared->child_pcb[process_index].last_run_time);
			writelog(msg);
			if_use_full_quantum = rand() % 10;     // randdom [O - 9]

			if (if_use_full_quantum < 2) {
				quantum_used = shared->quantum;
				sprintf(msg, "process %d using entire quantum\n", process_index);
			} else {
				// use part of quantum
				quantum_used = rand() % shared->quantum;
				sprintf(msg, "process %d using %d of quantum\n", process_index, quantum_used);
				writelog(msg);
			}

			// process total_system_time_ms
			shared->child_pcb[process_index].total_cpu_time_ms += quantum_used;
			// update PCB set last run time to this quantum_used
			shared->child_pcb[process_index].previous_burst_time = quantum_used;
			// update this process run_status
			shared->child_pcb[process_index].run_status = 1;
			// signal oss to schedule another process
			struct sembuf sbuf;                         // semaphore op struct
			sbuf.sem_num = 0;                           // only semaphore in set
			sbuf.sem_op = 1;                            // increment the semaphore
			sbuf.sem_flg = 0;                           // op flag

			if (semop(semid_oss, &sbuf, 1) == -1)  {
				if (errno == EINTR) {
					sprintf(msg, "process %02d: semop error\n", process_index);
					perror(msg);
					exit(EXIT_FAILURE);
				}
			}

			// process ran for  500 or more milliseconds
			if (shared->child_pcb[process_index].total_cpu_time_ms >= 500) {
				shared->child_pcb[process_index].done_status = 1;
				sprintf(msg, "process %02d ran 500 or more milliseconds...exiting\n" , process_index);
				writelog(msg);
			}
		}
	}

	return 0;
}

void signal_handler() {
	exit(0);
}


void signal_init() {
	struct sigaction sigint;
	sigint.sa_handler = signal_handler;
	sigint.sa_flags = 0;

	if (sigaction(SIGINT, &sigint, NULL) == -1) {
		perror("signal_init: SIGINT");
	}
}
