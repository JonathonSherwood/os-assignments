/* JON SHERWOOD: CS 4760 PROJECT 1 */
/* This is a file to test the logging library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "liblogger.h"


void usage() {
	printf ("USAGE: logtest -f <logfile>\n");
	printf ("if no logfile specified \"logfile\" \
	written in current directory\n");
	exit (EXIT_SUCCESS);
}

int main (int argc, char **argv) {
	char *logfile = NULL;
	int c, index = 0;

	/* the name of this process for the logger */
	processname = __FILE__;

	while ((c = getopt (argc, argv, "hf:")) != -1) {
		switch (c) {
		/* print help */
		case 'h':
			usage();
			break;

		/* check for logfile name */
		case 'f':
			logfile = optarg;
			break;

		case '?':

			/* -f with no logfile name */
			if (optopt == 'f') {
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
			abort();
		}
	}

	/* make sure no more arguments. if so exit */
	for (index = optind; index < argc; index++) {
		printf ("Non-option argument %s\n", argv[index]);
		exit (EXIT_FAILURE);
	}

	/* if no user defined logfile */
	if (logfile == NULL) {
		logfile = (char *)malloc (12);
		strcpy (logfile, "./logfile");
	}


	writelog ("First test message.\n");
	writelog ("Second test message.");
	sleep (1);
	writelog ("and another message ");
	writelog ("done testing ....");
	printf ("%s", getlog());
	savelog (logfile);
	clearlog();

	return 0;
}
