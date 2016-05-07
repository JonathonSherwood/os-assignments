/* JON SHERWOOD: CS4760 - PROJECT 3 */

#include "shared.h"                            

int signal_id;
int sleep_timeout;     // for user sleep timeout

void usage() {
	printf ("USAGE: master [options] \n\n");
	printf ("OPTIONS:\n");
	printf ("\t-h  help \n");
	printf ("\t-n  [# of children 1 to 19] default is 19 \n");
	printf ("\t-t  [# of secs for child timeout] default is 60 \n");
}

pid_t r_wait (int *stat_loc) {
	int retval;

	while ( ( (retval = wait (stat_loc)) == -1) && (errno == EINTR));

	return retval;
}

// catch and process signals
void sigproc (int sig) {
	if (sig == SIGINT) {
		fprintf (stderr, "\nCaught Ctrl-C\n");
		cleanup (SIGINT);
	}
}

void alarm_handler() {
	fprintf (stderr, "\nTimer Elapsed\n");
	cleanup (SIGINT);
}



int main (int argc, char **argv) {
	int i = 0;
	int c;
	int index = 0;
	num_children = -1;          // for user # of children
	sleep_timeout = -1;     // for user sleep timeout

	while ( (c = getopt (argc, argv, "hn:t:")) != -1) {
		switch (c) {
			// print help
			case 'h':
				usage();
				exit (EXIT_FAILURE);

			// user defined number of child processes
			case 'n':
				num_children = atoi (optarg);

				if ( (num_children < 1) || (num_children > 19)) {
					fprintf (stderr, "master: -n has invalid value.\n");
					exit (EXIT_FAILURE);
				}

				break;

			case 't':
				sleep_timeout = atoi (optarg);

				if ( (sleep_timeout < 1) || (sleep_timeout > 60)) {
					fprintf (stderr, "master: -h has invalid value.\n");
					exit (EXIT_FAILURE);
				}

				break;

			case '?':

				// -n with no num_children argument
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

	// Make sure no more arguments. if so exit
	for (index = optind; index < argc; index++) {
		printf ("Non-option argument %s\n", argv[index]);
		exit (EXIT_FAILURE);
	}

	if (num_children == -1) {
		num_children = DEFAULT_NUM_SLAVES;
	}

	if (sleep_timeout == -1) {
		sleep_timeout = DEFAULT_SLEEP_TIMEOUT;
	}


	// Signal initializations
	if (signal (SIGINT, sigproc) == SIG_ERR) {
		fprintf (stderr, "can't catch SIGINT\n");
		exit (EXIT_FAILURE);
	}

	if (signal (SIGALRM, &alarm_handler) == SIG_ERR) {
		fprintf (stderr, "can't catch SIGALRM\n");
		exit (EXIT_FAILURE);
	}

	alarm (sleep_timeout);

	// Fork and exec slaves
	for (i = 1; i <= num_children; i++) {
		slave_pid[i] = fork();

		if (slave_pid[i] == 0) {
			// String to hold slave argument
			char slave_arg[3];
			sprintf (slave_arg, "%d", i);
			execl ("./slave", "slave", slave_arg, (char *) NULL);

		} else if (slave_pid[i] < 0) {
			perror ("master: fork for slave");
			cleanup (SIGINT);
    		exit (EXIT_FAILURE);
		}
	}

	// Wait for all children
	while (r_wait (NULL) > 0) ;

	return 0;
}

