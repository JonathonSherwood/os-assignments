#include "shared.h"

void run (pcb_t *);
void finished (pcb_t *current_pcb);
int quantum_completion();

void sig_handler (int);
void sig_handler (int signo) {
	if (signo == SIGINT) {
		sprintf (msgstr, "User Process %d caught SIGINT - Exiting.\n", getpid());
		writelog (msgstr);
		exit (0);
	} else if (signo == SIGALRM) {
		sprintf (msgstr, "User Process %d - Killed.\n", getpid());
		writelog (msgstr);
		exit (0);
	}
}
shared_data *d;
int my_index;
int l = 0;

int main (int argc, char **argv) {
	key_t key = ftok ("./oss.c", 'A');
	int shmid;

	if ((shmid = shmget (key,  sizeof (shared_data), IPC_CREAT | 0666)) == -1) {
		perror ("Failed to create shared memory segment");
		exit (1);
	}

	if ((d = (shared_data *)shmat (shmid, NULL, 0)) == (void *) - 1) {
		perror ("Failed to attach to shared memory space");
		exit (1);
	}

	my_index = atoi (argv[1]);
	// SIGNALS
	struct sigaction act;
	act.sa_handler = sig_handler;
	act.sa_flags = 0;
	sigaction (SIGALRM, &act, NULL);
	sigaction (SIGINT, &act, NULL);
	// process control block pointer
	pcb_t *current_pcb = &d->pcb[my_index];
	srand (time (NULL));

	for (;;) {
		sem_wait (d->turn, my_index);
		run (current_pcb);
	}
}

void run (pcb_t *current_pcb) {
	int q = quantum_completion();
	current_pcb->cpu_time += q;
	current_pcb->previous_burst = q;
	finished (current_pcb);
	current_pcb->wait_time = 0;
	sem_signal (d->sem, 0);
	
}

int quantum_completion() {
	int q = d->quantum;
	int randomNum = rand() % 2;
	if (randomNum == 1) {  // use part of quantum
		randomNum = rand() % (q + 1);
		q = randomNum;
	}

	return q;
}

void finished (pcb_t *current_pcb) {
	if (current_pcb->cpu_time >=
			50000000) {   // 50ms.
			int randomNum = rand() % 2;

		if (randomNum == 1) {
			current_pcb->clear_pcb = 1;
			current_pcb->pid = 0;
			usleep (100000);
			sem_signal (d->sem, 0);
			exit (0);
		}
	}
}
