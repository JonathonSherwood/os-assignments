/* JON SHERWOOD: CS4760 - PROJECT 3 */
#include "shared.h"

int main (int argc, char *argv[]) {
	// We need the process index as an argment
	if (argc != 2) {
		fprintf (stderr, "Slave index was not passed in call\n");
		exit (EXIT_FAILURE);
	}

	int child_number = atoi (argv[1]);   // process index
	slave_pid[child_number] = getpid();
	int sleep_secs;    // Hold the random sleep time
	srand (time (NULL));

	for (; ;) {

		// sleep for 0-2 secs
		sleep_secs = rand() % 3;
		sleep (sleep_secs);

		monitor ( argv[1]);
		// sleep for 0-2 secs
		sleep_secs = rand() % 3;
		sleep (sleep_secs);
	}

	return 0;
}

