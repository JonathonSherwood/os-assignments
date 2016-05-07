#include <ctype.h>
#include "shared.h"

// Statistics
typedef struct {
	int x; // helper flag
	unsigned long num_procs_finished;
	unsigned long long avg_time_in_sys;
	unsigned long long avg_cpu_time;
	unsigned long long avg_burst_time;
} avg_t;

avg_t avg;
shared_data *d;

int scheduler();
void clear_pcb (pcb_t *pcb);
int insert_proc();
void time_totals (unsigned long long, unsigned long long, unsigned long long);
float nstoms (unsigned long long);
float nstos (unsigned long long);
int child_count();
int arghelper (int);
pid_t r_wait (int *);
void sig_handler (int);
void usage();
int shmid;
int pcb_flag[CHILD_MAX];
int quantum_val;


void cleanup() {
	while (r_wait (NULL) > 0) ; //wait for all children

	sprintf (msgstr, "Statistics\n");
	writelog (msgstr);
	fprintf (stderr, "%s", msgstr);
	sprintf (msgstr, "Averages for Completed Processes\n");
	writelog (msgstr);
	fprintf (stderr, "%s", msgstr);
	sprintf (msgstr, "Average Time in System: %.2fms\n",
			 nstoms (avg.avg_time_in_sys));
	writelog (msgstr);
	fprintf (stderr, "%s", msgstr);
	sprintf (msgstr, "Average CPU Time: %.2fms\n", nstoms (avg.avg_cpu_time));
	writelog (msgstr);
	fprintf (stderr, "%s", msgstr);
	sprintf (msgstr, "Average Burst Length: %.2fms\n\n",
			 nstoms (avg.avg_burst_time));
	writelog (msgstr);
	fprintf (stderr, "%s", msgstr);
	sprintf (msgstr, "Total Run Time: %ds %.2fms\n", d->shared_clock.sec,
			 nstoms (d->shared_clock.nano_sec));
	writelog (msgstr);
	fprintf (stderr, "%s", msgstr);
	semctl (d->sem, 0, IPC_RMID); //remove the semaphores
	int i;

	for (i = 0; i < CHILD_MAX; i++) {
		semctl (d->turn, i, IPC_RMID);
	}

	int error = 0;

	if (shmdt (d) == -1) {
		error = errno;
	}

	if ((shmctl (shmid, IPC_RMID, NULL) == -1) && !error) {
		error = errno;
	}

	if (error) {
		system ("./clean.sh");
		exit (EXIT_FAILURE);
	}
}

void sig_handler (int signo) {
	if (signo == SIGINT) {
		sprintf (msgstr, "OSS received SIGINT - Exiting.\n");
		writelog (msgstr);
		fprintf (stderr, "%s", msgstr);
	} else if (signo == SIGALRM) {
		int i;

		for (i = 0; i < CHILD_MAX; i++) {
			if (pcb_flag[i] == 1) {
				kill (d->pcb[i].pid, signo);
			}
		}

		writelog ("OSS - Out of time. Cleaning up and terminating.\n");
	}

	cleanup();
}

int main (int argc, char **argv) {
	int c;
	int i;
	quantum_val = -1;             // for user # of children
	int cpu_alloc_high_priority = -1;  // for user sleep timeout

	while ((c = getopt (argc, argv, "hp:n:")) != -1) {
		switch (c) {
			case 'h':           // print help
				usage();
				exit (EXIT_FAILURE);

			case 'p':           // user set priority
				cpu_alloc_high_priority = atoi (optarg);

				if ((cpu_alloc_high_priority < 0) || (cpu_alloc_high_priority > 100)) {
					fprintf (stderr, "oss: -p has invalid value.\n");
					exit (EXIT_FAILURE);
				}

				break;

			case 'q':
				quantum_val = atoi (optarg);

				if ((quantum_val < 1) || (quantum_val > 100)) {
					fprintf (stderr, "oss: -n has invalid value.\n");
					exit (EXIT_FAILURE);
				}

				break;

			case '?':
				if (optopt == 'p' || optopt == 'q') {
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
					exit (EXIT_FAILURE);
				} else if (isprint (optopt)) {
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
					exit (EXIT_FAILURE);
				} else {
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
					exit (EXIT_FAILURE);
				}

			default:
				exit (EXIT_FAILURE);
		}
	}

	// Make sure no more arguments. if so exit
	for (i = optind; i < argc; i++) {
		fprintf (stderr, "Non-option argument %s\n", argv[i]);
		exit (EXIT_FAILURE);
	}

	if (cpu_alloc_high_priority == -1) {
		cpu_alloc_high_priority = 80;
	}

	if (quantum_val == -1) {
		quantum_val = 10;
	}

	// quantum ms to nano_sec
	quantum_val *= 1000000;
	srand (time (NULL));
	alarm (GLOBAL_TIMEOUT);
	//////////////////////////////
	// SHARED MEMORY
	key_t shmkey = ftok ("./oss.c", 'A');
	int shmem = sizeof (shared_data);

	if ((shmid = shmget (shmkey, shmem, IPC_CREAT | 0666)) == -1) {
		perror ("Failed to create shared memory segment");
		exit (1);
	}

	if ((d = (shared_data *)shmat (shmid, NULL, 0)) == (void *) - 1) {
		perror ("Failed to attach to shared memory space");
		exit (1);
	}

	d->shared_clock.sec = 0;
	d->shared_clock.nano_sec = 0;

	//initialize the flags and PCBs
	for (i = 0; i < CHILD_MAX; i++) {
		pcb_flag[i] = 0;
		clear_pcb (&d->pcb[i]); //reset the pcb_t.
	}

	d->quantum = 0;
	//////////////////////////////
	// SEMAPHORE
	key_t semkey1 = ftok ("./oss.c", 'A');
	int semid1;

	if ((semid1 = semget (semkey1, 1, 0700 | IPC_CREAT)) == -1) {
		perror ("Failed to access sempahore");
		exit (1);
	}

	semctl (semid1, 0, SETVAL, 0);
	semctl (semid1, 1, SETVAL, 0);
	d->sem = semid1;

	int semid2;
	key_t semkey2 = ftok ("./oss.c", 'B');

	if ((semid2 = semget (semkey2, CHILD_MAX, 0700 | IPC_CREAT)) == -1) {
		perror ("Failed to access sempahore");
		exit (1);
	}

	// SEMAPHORE FOR EACH CHILD/SLAVE
	for (i = 0; i <= CHILD_MAX; i++) {
		semctl (semid2, i, SETVAL, 0);
	}

	d->turn = semid2;
	avg.num_procs_finished = 0;
	avg.avg_time_in_sys = 0;
	avg.avg_cpu_time = 0;
	avg.x = 1;
// initialize signals
	struct sigaction act;
	act.sa_handler = sig_handler;
	act.sa_flags = 0;
	sigaction (SIGALRM, &act, NULL);
	sigaction (SIGINT, &act, NULL);
	unsigned long long fork_time = 1000000000; //gen a new processes every sec
	unsigned long long tmNextFork = fork_time;
	pcb_t *pcb;

	while (1) {
		// random from 0-1000
		unsigned int nano_sec = rand() % 1001;
		tmNextFork += nano_sec;
		d->shared_clock.nano_sec += nano_sec;

		// check for seconds
		if (d->shared_clock.nano_sec >= 1000000000) {
			d->shared_clock.sec++;
			d->shared_clock.nano_sec -= 1000000000;
		}

		// update time values
		for (i = 0; i < CHILD_MAX; i++) {
			// if a process control block exist at index i
			if (pcb_flag[i] == 1) {
				d->pcb[i].sys_time += nano_sec; //update the total time in system
				d->pcb[i].wait_time += nano_sec; //update time waiting since last scheduled
			}
		}

		/*if it's time to fork (total seconds equals or passed time for next fork) and
		/if there's an open slot*/
		if (tmNextFork >= fork_time) { //if it's time to fork again
			int open = insert_proc();

			if (open >= 0) { //if there's a slot open
				tmNextFork = 0;
				int pid;
				pcb_flag[open] = 1;
				pid = fork();

				if (pid == 0) {
					d->pcb[open].pid = getpid();
					sprintf (msgstr, "OSS Process number %d with pid %d Generated.\n", open,
							 d->pcb[open].pid);
					writelog (msgstr);
					char *arg = malloc (arghelper (open));
					sprintf (arg, "%d", open);
					execl ("slave", "slave", arg, NULL); //exec a user process
					perror ("Failed to exec a user process.");
				} else if (pid == -1) {
					pcb_flag[open] = 0;
					perror ("Failed to fork.");
					sleep (1);
				}
			}
		}

		int index = scheduler();

		if (index != -1) { //if there's a process that was scheduled
			pcb = &d->pcb[index];
			sprintf (msgstr,
					 "OSS Process number %d with pid %d scheduled CPU.   Given Quantum: %.2fms\n",
					 index, pcb->pid, nstoms (d->quantum));
			writelog (msgstr);
			sem_wait (d->sem, 0);
			sprintf (msgstr,
					 "OSS Process number %d with pid %d surrendered CPU. Used Quantum:  %.2fms\n",
					 index, pcb->pid, nstoms (pcb->previous_burst));
			writelog (msgstr);

			if (d->pcb[index].clear_pcb == 1) { //if this process said it's clear_pcb...
				sprintf (msgstr, "\n============\n");
				writelog (msgstr);
				sprintf (msgstr, "OSS Process number %d with pid %d exiting system.\n", index,
						 pcb->pid);
				writelog (msgstr);
				sprintf (msgstr, "Final Report\n");
				sprintf (msgstr, "Time In System: %.2fms\nTotal Time On CPU: %.2fms\n",
						 nstoms (pcb->sys_time), nstoms (pcb->cpu_time));
				writelog (msgstr);
				sprintf (msgstr, "============\n\n");
				writelog (msgstr);
				time_totals (pcb->sys_time, pcb->cpu_time, pcb->previous_burst);
				clear_pcb (pcb);
				pcb_flag[index] = 0; //reset the flag for this process
				semctl (d->turn, index, SETVAL, 0); //reset the semaphore for this process
				r_wait (NULL); //wait for this child.
			}

			d->shared_clock.nano_sec += d->quantum;

			if (d->shared_clock.nano_sec >= 1000000000) {
				d->shared_clock.sec++;
				d->shared_clock.nano_sec -= 1000000000; //remove 1 billion nanoseconds
			}

			int i;

			for (i = 0; i < CHILD_MAX; i++) {
				if (pcb_flag[i] == 1) {
					d->pcb[i].sys_time += nano_sec; // update the total time in system
					d->pcb[i].wait_time += nano_sec; //update time waiting since last scheduled
				}
			}

			tmNextFork += d->quantum;
		}
	}

	cleanup();
	return 0;
}



int scheduler() {
	if (child_count() == 0) { //if no processes currently exist...
		return -1;
	}

	int i;
	unsigned long long max = 0;
	int last_index = 0;
	unsigned long long weights[CHILD_MAX];

	//calculate weights
	for (i = 0; i < CHILD_MAX; i++) {
		if (pcb_flag[i] == 1) {
			unsigned long long tw = d->pcb[i].wait_time;
			unsigned long long time_in_system = d->pcb[i].sys_time;
			unsigned long long total_cpu = d->pcb[i].cpu_time;
			unsigned long long weight = (time_in_system / 2) + ((time_in_system / 2) / 10)
										+ (tw / 2) -
										(total_cpu * 10 / 2);
			weights[i] = weight;
		} else {
			weights[i] = 0;
		}
	}

	//find the max
	for (i = 0; i < CHILD_MAX; i++) {
		if (weights[i] > max) {
			max = weights[i];
			last_index = i;
		}
	}

	d->quantum = quantum_val;

	// schedule process
	if (pcb_flag[last_index] == 1
			&& d->pcb[last_index].pid != 0) {
		sem_signal (d->turn,
					last_index); //signal and run process
		return last_index;
	}

	return -1;
}


void time_totals (unsigned long long time_in_system,
				  unsigned long long total_cpu,
				  unsigned long long total_burst_time) {
	avg.num_procs_finished++;
	avg.avg_time_in_sys += time_in_system;
	avg.avg_cpu_time += total_cpu;
	avg.avg_burst_time += total_burst_time;

	if (avg.x == 1) {
		avg.x = 0;
	} else {
		avg.avg_time_in_sys /= 2;
		avg.avg_cpu_time /= 2;
		avg.avg_burst_time /= 2;
	}
}



//return the index of an open slot for the next pcb_t, else return -1 if all slots are full.
int insert_proc() {
	int i;

	for (i = 0; i < CHILD_MAX; i++) {
		if (pcb_flag[i] == 0) {
			return i;
		}
	}

	return -1;
}



void clear_pcb (pcb_t *pcb) {
	pcb->pid = 0;
	pcb->cpu_time = 0;
	pcb->sys_time = 0;
	pcb->wait_time = 0;
	pcb->previous_burst = 0;
	pcb->clear_pcb = 0; //initialize all values to zero.
}

//function to take nanoseconds and return its value in seconds
float nstos (unsigned long long nano_sec) {
	float s = (float)nano_sec / 1000000000.0;
	return s;
}

//function to take nanoseconds and return its value in milliseconds
float nstoms (unsigned long long nano_sec) {
	float ms = (float)nano_sec / 1000000.0;
	return ms;
}

int arghelper (int num) {
	int digits = 0;

	while (num != 0) {
		num /= 10;
		digits++;
	}

	return digits;
}

//return the number of processes currently in the system
int child_count() {
	int i;
	int tot = 0;

	for (i = 0; i < CHILD_MAX; i++) {
		if (pcb_flag[i] == 1) {
			tot++;
		}
	}

	return tot;
}
void usage() {
	printf ("USAGE: oss [options] \n\n");
	printf ("OPTIONS:\n");
	printf ("\t-h  help \n");
	printf ("\t-q  quantum value in milliseconds \n");
	printf ("\t-p  [%% of cpu allocated to high priority] default is 80 \n");
}


pid_t r_wait (int *stat_loc) {
	int retval;

	while (((retval = wait (stat_loc)) == -1) && (errno == EINTR));

	return retval;
}
