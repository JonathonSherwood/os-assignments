/* JON SHERWOOD: CS4760 - PROJECT 2 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <wait.h>
#include <fcntl.h>
#include "shared.h"



/*MAX TIMES PROCESS ENTERS THE CRITICAL SECTION AND WRITES TO CSTEST*/
#define MAX_WRITES 3
#define FILE_PERMS (S_IRWXU | S_IRWXG | S_IRWXO )
#define FILE_FLAGS (O_CREAT |O_RDWR |O_APPEND|O_SYNC)

/*CATCH AND PROCESS SIGNALS*/
void sigproc (int sig) {
	signal_id = sig;
}

/* WRITE MESSAGE TO LOGFILE "CSTEST" */
void writelog (char *msg) {
	/*GET THE TIME IN STRING FORM*/
	time_t now;
	time (&now);
	char *timestr = (char *)malloc (6);
	strftime (timestr, 6, "%I:%M", localtime (&now));
	char tmp[52];
	sprintf (tmp, "File modified by process number %s at time %s\n", msg, timestr);
	int fd = open ("cstest", FILE_FLAGS, 0777);

	if (fd < 0) {
		perror ("Open failed");
		exit (1);
	}

	write (fd, tmp, strlen (tmp));
	write (STDOUT_FILENO, tmp, strlen (tmp));
	close (fd);
}

int main (int argc, char *argv[]) {
	int sleep_secs;      /*RANDOM SLEEP TIME*/
	int num_children;    /*PROCESS INDEX*/
	int shmid_tflg;     /*SHARED MEMORY ID FOR TURN FLAG*/
	int turn;
	int j;
	char error_msg[50];

	/*WE NEED THE PROCESS INDEX AS AN ARGMENT*/
	if (argc != 2) {
		fprintf (stderr, "no value passed to slave\n");
		exit (1);
	} else {
		num_children = atoi (argv[1]);
	}

	/* SHARED MEMORY SETUP */
	key_t key;
	key_t key2;

	/*MAKE THE KEYS*/
	if ((key = ftok (".", 'A')) == -1) {
		perror ("ftok");
		exit (EXIT_FAILURE);
	}

	if ((key2 = ftok (".", 'B')) == -1) {
		perror ("ftok");
		exit (EXIT_FAILURE);
	}

	shmid_tflg = shmget (key, sizeof (int), 0644 | IPC_CREAT);

	if (shmid_tflg == -1) {
		sprintf (error_msg, "\nSlave %d: shmget (turn flag)", num_children);
		perror (error_msg);
		return 1;
	}

	/*GET A POINTER TO THE SHARED TURN FLAG SEGMENT*/
	int *tflg = (int *) (shmat (shmid_tflg, 0, 0));

	if (! tflg) {
		sprintf (error_msg, "\nSlave %d: shmat (turn flag)", num_children);
		perror (error_msg);
		return 1;
	}

	/*GET THE ID OF THE SHARED STATE FLAG SEGMENT*/
	turn = shmget (key2, sizeof (shared_mem_flag), 0644 | IPC_CREAT);

	if (turn == -1) {
		sprintf (error_msg, "\vSlave %d: shmget (state flag array)", num_children);
		perror (error_msg);
		return 1;
	}

	/*Get a pointer to the shared state flag segment*/
	shared_mem_flag *sflg = (shared_mem_flag *) (shmat (turn, 0, 0));

	if (! sflg) {
		sprintf (error_msg, "\nSlave %d: shmat (state flags)", num_children);
		perror (error_msg);
		return 1;
	}

	/*SET N FROM THE LAST POSITION OF THE STATE FLAGS*/
	int n = sflg->shared_mem_flag[18];

	for (; ;) {
		/*LOOK FOR SIGNALS SIGINT / CTRL-C AND SIGTERM / NORMAL EXIT*/
		signal (SIGINT, sigproc);
		signal (SIGTERM, sigproc);

		if (signal_id) {
			if (signal_id == 2) {
				fprintf (stderr, "slave %d caught CTRL-C\n", num_children);
			}

			break;
		}

		/* ENTRY SECTION */
		int i;

		for (i = 0; i < MAX_WRITES; i++) {
			do {
				sflg->shared_mem_flag[num_children] = want_in; /*RAISE MY FLAG*/
				j = *tflg;                          /*SET THE LOCAL TURN FLAG*/

				while (j != num_children) {
					j = (sflg->shared_mem_flag[j] != idle) ? *tflg : (j + 1) % n;
				}

				/*DECLARE INTENTION TO ENTER CRITICAL SECTION*/
				sflg->shared_mem_flag[num_children] = in_cs;

				/*CHECK IF NO ONE ELSE IS IN CS*/
				for (j = 0; j < n; j++)
					if ((j != num_children) && (sflg->shared_mem_flag[j] == in_cs)) {
						break;
					}
			} while ((j < n) || (*tflg != num_children
								 && sflg->shared_mem_flag[*tflg] != idle));

			/*GET TURN.*/
			*tflg = num_children;
			/* CRITICAL SECTION */
			time_t now;
			time (&now);
			char *timestr = (char *)malloc (6);
			strftime (timestr, 6, "%I:%M", localtime (&now));
			fprintf (stderr, "Slave %d entered critical section at %s\n", num_children,
					 timestr);
			/*SLEEP RANDOMLY FOR 0-2 SECONDS*/
			srand (time (NULL));
			sleep_secs = rand() % 3;
			sleep (sleep_secs);
			/* WRITE INTO THE FILE*/
			writelog (argv[1]);
			/*SLEEP RANDOMLY FOR 0-2 SECONDS*/
			sleep_secs = rand() % 3;
			sleep (sleep_secs);
			/* EXIT SECTION */
		}

		j = (*tflg + 1) % n;

		while (sflg->shared_mem_flag[j] == idle) {
			j = (j + 1) % n;
		}

		/*ASSIGN TURN TO NEXT WAITING PROCESS, CHANGE MY FLAG TO IDLE*/
		*tflg = j;
		sflg->shared_mem_flag[num_children] = idle;
		/* REMAINDER SECTION */
	}

	/*DETACH STATE FLAG*/
	if (shmdt (sflg) == -1) {
		sprintf (error_msg, "slave %d: detach shared state flag", num_children);
		perror (error_msg);
	}

	/*DETACH TURN FLAG*/
	if (shmdt (tflg) == -1) {
		sprintf (error_msg, "slave %d: detach shared turn flag", num_children);
		perror (error_msg);
	}

	return 0;
}
