/* JON SHERWOOD: CS4760 - PROJECT 2 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <wait.h>
#include <string.h>
#include <ctype.h>

#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include "shared.h"

#define SFLG_SZ (sizeof(shared_mem_flag))   /*SET THE SIZE OF THE STATE FLAG STRUCT*/
#define DEFAULT_NUM_CHILDREN 2
#define DEFAULT_MAX_PROC 19
#define DEFAULT_SLEEP_TIMEOUT 60

/*GLOBAL VARIABLES*/
int num_children;
pid_t slave_pid[18];
int shmid_tflg;     /*SHARED MEMORY FLAG*/
int state_flag;     /*SHARED MEMORY TURN*/

/*CATCH AND PROCESS SIGNALS*/
void sigproc (int sig) {
	signal_id = sig;
}

/*TERMINATE ALL PROCESSES AND FREE SHARED MEMORY*/
void cleanup (int termsig) {
	int status;
	int i;

	for (i = 0; i < num_children; i++) {
		fprintf (stderr, "Attempting to terminate slave %d\n", i);
		kill (slave_pid[i], termsig);
		slave_pid[i] = wait (&status);
		fprintf (stderr, "Slave %d returned %d\n", i, WEXITSTATUS (status));
	}

	int tflg_ret = shmctl (shmid_tflg, IPC_RMID, (struct shmid_ds *)NULL);

	if (tflg_ret != 0) {
		fprintf (stderr,
				 "Error releasing shared state_flag flag - please clear manually\n");
	}
}



int main (int argc, char **argv) {
	signal_id = 0;
	int i = 0;
	int c;
	int index = 0;
	num_children = -1;          /*FOR USER # OF CHILDREN*/
	int sleep_timeout = -1;     /*FOR USER SLEEP TIMEOUT*/
	key_t key;
	key_t key2;

	while ((c = getopt (argc, argv, "hn:t:")) != -1) {
		switch (c) {
			/* PRINT HELP*/
			case 'h':
				usage();
				exit (EXIT_FAILURE);

			/* USER DEFINED NUMBER OF CHILD PROCESSES*/
			case 'n':
				num_children = atoi (optarg);

				if ((num_children < 1) || (num_children > 19)) {
					fprintf (stderr, "master: -n has invalid value.\n");
					exit (EXIT_FAILURE);
				}

				break;

			case 't':
				sleep_timeout = atoi (optarg);

				if ((sleep_timeout < 1) || (sleep_timeout > 60)) {
					fprintf (stderr, "master: -h has invalid value.\n");
					exit (EXIT_FAILURE);
				}

				break;

			case '?':

				/*-n WITH NO NUM_CHILDREN ARGUMENT*/
				if (optopt == 'n' || optopt == 't') {
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

	/*MAKE SURE NO MORE ARGUMENTS. IF SO EXIT*/
	for (index = optind; index < argc; index++) {
		printf ("Non-option argument %s\n", argv[index]);
		exit (EXIT_FAILURE);
	}

	if (num_children == -1) {
		num_children = DEFAULT_NUM_CHILDREN;
	}

	if (sleep_timeout == -1) {
		sleep_timeout = DEFAULT_SLEEP_TIMEOUT;
	}

	/* SHARED MEMORY SETUP */

	if ((key = ftok (".", 'A')) == -1) {
		perror ("ftok");
		exit (EXIT_FAILURE);
	}

	if ((key2 = ftok (".", 'B')) == -1) {
		perror ("ftok");
		exit (EXIT_FAILURE);
	}

	/*ALLOCATE MEMORY FOR TURN FLAG*/
	shmid_tflg = shmget (key, sizeof (int), 0644 | IPC_CREAT | IPC_CREAT);

	if (shmid_tflg == -1) {
		perror ("master: shmget: state_flag flag");
		exit (1);
	}

	/*ALLOCATE MEMORY FOR STATE FLAGS*/
	state_flag = shmget (key2, sizeof (shared_mem_flag),
						 0644  | IPC_CREAT);

	if (state_flag == -1) {
		perror ("master: shmget: state flags");
		exit (1);
	}

	/* SET THE LAST ELEMENT OF THE SHARED MEMORY TO NUM_CHILDREN */
	shared_mem_flag *sflg = (shared_mem_flag *) (shmat (state_flag, 0, 0));

	if (! sflg) {
		perror ("master: shmat (state flags)");
		return 1;
	}

	sflg->shared_mem_flag[18] = num_children;

	if (shmdt (sflg) == -1) {
		perror ("master: shmdt (state flags)");
	}

	for (i = 0; i < num_children; i++) {
		slave_pid[i] = fork();

		if (slave_pid[i] == 0) {
			/*STRING TO HOLD SLAVE ARGUMENT*/
			char slave_arg[3];
			sprintf (slave_arg, "%d", i);
			execl ("./slave", "slave", slave_arg, (char *)NULL);
			perror ("master: slave exec");
		} else if (slave_pid[i] < 0) {
			perror ("fork() for slave");
			cleanup (SIGINT);
			exit (1);
		}
	}

	/*CTRL-C SIGNAL OR 60 SECONDS*/
	for (i = 0; i < sleep_timeout; i++) {
		signal (SIGINT, sigproc);

		/*INTERRUPT (CTRL-C WAS RECEIVED)*/
		if (signal_id) {
			break;
		}

		sleep (1);
	}

	if (i == sleep_timeout) {
		fprintf (stderr, "Timer Elapsed\n");
		cleanup (SIGTERM);
	} else {
		fprintf (stderr, "Caught Ctrl-C\n");
		cleanup (SIGINT);
	}

	return 0;
}



