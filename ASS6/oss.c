/* JON SHERWOOD - CS 4760 - ASSIGNMENT 6 - MEMORY MANAGEMENT */
#include "shared.h"
#define BILLION 1000000000L

void signal_init();         // initialize signal handlers
void signal_handler();      // handle signals
void alarm_handler();       // handles alarm print memory every sec
void page_table_signal() ;

void init_sem();            // semaphore and shared mem functions
void remove_sem();
void shared_mem_init();
void shared_mem_attach();
void remove_shared_memory();

pid_t r_wait(int *);        // wait for children during cleanup
int lru_find_oldest();      // find oldest page for victim
void page_table_set(int);
void page_faults();          // handle page faults
void queue_rearrange();
int open_slot();             // find open slot
void display_pages_memory(); // prints output
void cleanup();              // clean up memory

key_t global_key = IPC_PRIVATE;
page_table_t *page_table;
mem_reference_t *mem_reference;
user_page_t *page;

sem_t *clock_sem;
sem_t *mem_ref_sem;
sem_t *page_table_sem;
sem_t *oss_sem;
page_data_t *page_queue[MAX_FRAMES];
logical_clock_t *logical_clock;


int shm_clock_id;
int shm_page_table_id;
int mem_ref_id;
int page_id;
int sem_clock_id;
int sem_ref_id;
int sem_page_table_id;
int sem_oss_id;
int tail = 0;
int number_of_procs;
struct timeval tv;

int main(int argc, char **argv) {
	char msg[50];           // for printing
	num_page_faults = 0;
	number_requests = 0;
	int i;
	int c;
	const char     *short_opt = "hn:";
	number_of_procs = DEFAULT_PROCESS_COUNT; // if no input set to 12
	struct option   long_opt[] = {
		{"help",          no_argument,       NULL, 'h'},
		{"number",        required_argument, NULL, 'n'},
		{NULL,            0,                 NULL, 0  }
	};

	while ((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)   {
		switch (c) {
			case -1:        //  no more arguments
			case  0:        //  long options toggles
				break;

			case 'n':

				/* VALIDATE NUMERIC INPUT */
				// check if -n or --number is zero
				if (isdigit(optarg[0]) && optarg[0] == '0') {
					fprintf(stderr, "Invalid input: %c \n", optarg[0]);
					fprintf(stderr, "Try -h or --help for options \n");
					return (-2);
				}

				// check for non numeric input
				for (i = 0; i < strlen(optarg); i++) {
					if (!isdigit(optarg[i])) {
						fprintf(stderr, "Invalid input: %c not a digit \n", optarg[i]);
						fprintf(stderr, "Try -h or --help for options \n");
						return (-2);
					}
				}

				number_of_procs = atoi(optarg);
				break;

			// help
			case 'h':
				fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
				fprintf(stderr, "  -n, --number              number of processeses\n");
				fprintf(stderr, "  -h, --help                print this help and exit\n\n");
				return (0);

			case ':':
			case '?':
				if (optopt == 'n') {
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
					exit(EXIT_FAILURE);
				} else if (isprint(optopt)) {
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
					exit(EXIT_FAILURE);
				} else {
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
					exit(EXIT_FAILURE);
				}

			default:
				abort();
		}
	}

	//  make sure no more arguments. if so exit
	for (i = optind; i < argc; i++) {
		fprintf(stderr, "Non-option argument %s\n", argv[i]);
		exit(EXIT_FAILURE);
	}

	// initialize signals and alarm
	signal_init();
	alarm(1);
	// initialize shared memory and semaphores
	shared_mem_init();
	shared_mem_attach();
	init_sem();

	/* forking */
	//  fix and allow more processeses to be queued for hard limit of 18 in system
	for (i = 0; i < number_of_procs; i++) {
		// fork process randomly 1 - 500 ms
		int random_fork_time = (1000000 + rand() % 500000000);

		// increment clock
		if (sem_wait(clock_sem) == -1) {
			perror("sem_wait: Failed to wait on clock_sem");
			exit(EXIT_FAILURE);
		}

		logical_clock->nsec += random_fork_time;

		if (logical_clock->nsec >= 1000000000) {
			logical_clock->sec++;
			logical_clock->nsec -= 1000000000;
		}

		float s = (((float)logical_clock->sec * 1000000000.0) + ((float)logical_clock->nsec)) / 1000000000.0;
		sprintf(msg, "Logical Clock Now:  %4.3f:\n",    s);
		writelog(msg);                            // logfile

		// write(STDOUT_FILENO, msg, strlen(msg));   // terminal
		if (sem_post(clock_sem) == -1) {
			perror("sem_post: Failed to post on clock_sem");
			exit(EXIT_FAILURE);
		}

		sprintf(msg, "Forking child #%d\n", i);
		writelog(msg);                            // logfile
		// write(STDOUT_FILENO, msg, strlen(msg));   // terminal
		pid_t pid = fork();

		if (pid  == -1) {
			perror("fork");
			return 1;
		} else if (pid == 0) {
			pids[i] = getpid();
			sprintf(msg, "user: #%2d pid: %d running\n", i,  pids[i]);
			writelog(msg);                            // logfile
			char arg1[10];
			sprintf(arg1, "%d", i);
			char arg2[10];
			sprintf(arg2, "%d", shm_clock_id);
			char arg3[10];
			sprintf(arg3, "%d", sem_clock_id);
			char arg4[10];
			sprintf(arg4, "%d", shm_page_table_id);
			char arg5[10];
			sprintf(arg5, "%d", sem_page_table_id);
			char arg6[10];
			sprintf(arg6, "%d", mem_ref_id);
			char arg7[10];
			sprintf(arg7, "%d", sem_ref_id);
			char arg8[10];
			sprintf(arg8, "%d", page_id);
			char arg9[10];
			sprintf(arg9, "%d", sem_oss_id);
			execl("user", "user", arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, NULL);
			perror("exec user");
			return 1;
		}
	}

	while (r_wait(NULL) > 0) ; // wait for children

	fprintf(stderr, "\noss: exiting normally \n");
	remove_sem();
	remove_shared_memory();
	system("./clean.sh");
	return 0;
}



void signal_init() {
	struct sigaction sigint;        // sigint gsignal
	sigint.sa_handler = signal_handler;
	sigint.sa_flags = 0;

	if (sigaction(SIGINT, &sigint, NULL) == -1) {
		perror("signal_init: SIGINT");
	}

	struct sigaction page_table_sig;    // page table signal
	page_table_sig.sa_handler = page_table_signal;
	page_table_sig.sa_flags = 0;

	if (sigaction(SIGUSR1, &page_table_sig, NULL) == -1) {
		perror("signal_init: SIGUSR");
		exit(EXIT_FAILURE);
	}

	// alarm
	if (signal(SIGALRM, &alarm_handler) == SIG_ERR) {
		perror("signal_init: unable to set SIGALARM");
		exit(EXIT_FAILURE);
	}
}

void signal_handler() {
	char msg[50];           // for printing
	sprintf(msg, "\nCaught CONTROL-C\n");
	writelog(msg);                            // logfile
	write(STDOUT_FILENO, msg, strlen(msg));   // terminal
	cleanup();
}

void alarm_handler() {
	alarm(1);
	display_pages_memory();
}

void page_table_signal() {
	char msg[50];
	int rw = mem_reference->rw;     // read - 0, write - 1
	int address = mem_reference->address;
	int child_num = mem_reference->child_index;

	sprintf(msg, "Checking Page Reference\n");
	writelog(msg);                            // logfile
	int i;
	number_requests++;

	for (i = 0; i < MAX_FRAMES; i++) {
		int index = page_table->entries[i].child_index;
		int page  = page_table->entries[i].page_num;
		int valid = page_table->entries[i].valid;

		/*hit: no page fault, oss just increments the clock by 10*/
		/*nanoseconds and sends a signal on the corresponding semaphore.*/
		if ((index == child_num) && (page == address) && (valid == 1)) {
			sprintf(msg, "Hit - Page %d Found [%d]\n", page, i);
			writelog(msg);                            // logfile
			write(STDOUT_FILENO, msg, strlen(msg));   // terminal

			/*increment clock 10ms*/
			if (sem_wait(clock_sem) == -1) {
				perror("sem_wait: Failed to wait on clock_sem");
				exit(EXIT_FAILURE);
			}

			logical_clock->nsec += 10; // 10 ns

			if (logical_clock->nsec >= 1000000000) {
				logical_clock->sec++;
				logical_clock->nsec -= 1000000000;
			}

			/*update time of page reference*/
			u_int64_t now = logical_clock->sec * 1000000000 + logical_clock->nsec;
			page_table->entries[i].reference_time = now;

			if (sem_post(clock_sem) == -1) {
				perror("sem_post: Failed to post on clock_sem");
				exit(EXIT_FAILURE);
			}


			if (sem_post(oss_sem) == -1) {
				perror("sem_post: oss sem");
				exit(EXIT_FAILURE);
			}
		}
	}

	num_page_faults++;
	/*(1-p) * mem access + p(15 ms[swap overhead])*/
	effective_access_time = ((1 - num_page_faults) * 10) + (num_page_faults * (15 * 1000000));
	sprintf(msg, "Effective access time: %llu nanoseconds:\n",    effective_access_time);
	writelog(msg);                            // logfile
	write(STDOUT_FILENO, msg, strlen(msg));   // terminal
	/* Page Fault [MISS]: oss queues the request to the device. */
	/* Each request for disk read / write takes about 15ms to be fulfilled. */
	sprintf(msg, "Page Fault \n");
	writelog(msg);                            // logfile
	write(STDOUT_FILENO, msg, strlen(msg));// terminal

	if (rw == 0) {
		sprintf(msg, "Read [%d]\n", i);
		writelog(msg);                          // logfile
		write(STDOUT_FILENO, msg, strlen(msg)); // terminal
		page_table->entries[i].dirty_bit = 0;
		page_table->entries[i].reference = 1;
	}

	if (rw == 1) {
		sprintf(msg, "Write [%d]\n", i);
		writelog(msg);                            // logfile
		// write(STDOUT_FILENO, msg, strlen(msg));   // terminal
		page_table->entries[i].reference = 1;
		page_table->entries[i].dirty_bit = 1;
	}

	// increment clock 10ms
	if (sem_wait(clock_sem) == -1) {
		perror("sem_wait: Failed to wait on clock_sem");
		exit(EXIT_FAILURE);
	}

	logical_clock->nsec +=  1.5e+7; // 15 ms for read / write

	if (logical_clock->nsec >= 1000000000) {
		logical_clock->sec++;
		logical_clock->nsec -= 1000000000;
	}

	if (sem_post(clock_sem) == -1) {
		perror("sem_post: Failed to post on clock_sem");
		exit(EXIT_FAILURE);
	}

	/*handle page faults*/
	page_faults();

	if (sem_post(oss_sem) == -1) {
		perror("sem_post: Failed to post on oss_sem");
		exit(EXIT_FAILURE);
	}
}



void page_faults() {
	char msg[50];

	if (sem_wait(page_table_sem) == -1) {
		perror("sem_wait: Failed to wait on page_table_sem");
		exit(EXIT_FAILURE);
	}

	int page_index;

	/*when all pages are valid and in use, there is no available page*/
	if ((page_index = open_slot()) == -1) {
		sprintf(msg, "All frames full. Page swap\n");
		writelog(msg);                            // logfile
		write(STDOUT_FILENO, msg, strlen(msg));   // terminal
		/* then, lru_find_oldest picks a page to kick out.*/
		page_table_set(lru_find_oldest());
	} else {
		page_table_set(page_index);
	}

	if (sem_post(page_table_sem) == -1) {
		perror("sem_post: Failed to signal page_table_sem");
		exit(EXIT_FAILURE);
	}
}

int open_slot() {
	int i;

	for (i = 0; i < MAX_FRAMES; i++) {
		if (page_table->entries[i].valid == 0) {
			return i;
		}
	}

	return -1;
}

int lru_find_oldest() {
	char msg[50];
	int i;
	int max_time_index = 0;

	// find oldest victim
	for (i = 1; i < MAX_FRAMES; i++) {
		if (page_table->entries[i].reference_time > page_table->entries[i - 1].reference_time) {
			max_time_index = i;
		}

		int table_index = page_queue[max_time_index]->table_index;

		if (page_queue[max_time_index]->reference == 1) {
			sprintf(msg, "Saved: [%d]\n", table_index);
			writelog(msg);
			write(STDOUT_FILENO, msg, strlen(msg));   // write to terminal
			page_queue[max_time_index]->reference = 0;
		} else if (page_queue[max_time_index]->reference == 0) {
			sprintf(msg, "Removed: [%d]\n", table_index);
			writelog(msg);
			write(STDOUT_FILENO, msg, strlen(msg));   // write to terminal
			page_table->entries[table_index].valid = 0;
			page_queue[max_time_index] = NULL;
			queue_rearrange(max_time_index);
			return table_index;
		}
	}

	return max_time_index;
}

void page_table_set(int page_index) {
	char msg[50];
	sprintf(msg, "Adding page %2d into  [%2d]\n", page->page_num, page_index);
	writelog(msg);
	write(STDOUT_FILENO, msg, strlen(msg));   // write to terminal
	page_table->entries[page_index].page_num = page->page_num;
	page_table->entries[page_index].child_index = page->child_index;
	page_table->entries[page_index].range[0] = page->range[0];
	page_table->entries[page_index].range[1] = page->range[1];
	page_table->entries[page_index].valid = 1;

	/*update reference time for page*/
	u_int64_t now = logical_clock->sec * 1000000000 + logical_clock->nsec;
	page_table->entries[page_index].reference_time = now;

	sprintf(msg, "process: pid %d reference_time = %lu\n", getpid(), now);
	writelog(msg);                            // logfile
	write(STDOUT_FILENO, msg, strlen(msg));   // terminal

	// make queue to proper size
	if (tail < MAX_FRAMES) {
		page_queue[tail] = &(page_table->entries[page_index]);
		page_queue[tail]->table_index = page_index;
		tail++;
	}
}


void queue_rearrange(int index) {
	int i;

	for (i = index; i < MAX_FRAMES; i++) {
		if (i == tail) {
			page_queue[i] = NULL;
			break;
		} else {
			page_queue[i] = page_queue[i + 1];
		}
	}

	tail--;
}

// print memory map of pages
void display_pages_memory() {
	char msg[100];
	int i;
	sprintf(msg, "\n..............................................................................................");
	write(STDOUT_FILENO, msg, strlen(msg)); // terminal
	sprintf(msg,  "\nCurrent Memory Map\n");
	write(STDOUT_FILENO, msg, strlen(msg));

	for (i = 0; i < MAX_FRAMES; i++) {
		// display unallocated frames by a period.
		if (page_table->entries[i].valid == 0) {
			sprintf(msg, "[%02d]       .       ", i);
			write(STDOUT_FILENO, msg, strlen(msg)); // terminal
		} else {
			sprintf(msg, "[%2d] user:%2d pg:%2d ", i, page_table->entries[i].child_index, page_table->entries[i].page_num);
			write(STDOUT_FILENO, msg, strlen(msg));  // terminal
		}

		// newline every 5
		if (((i + 1) % 5) == 0) {
			sprintf(msg, "\n");
			write(STDOUT_FILENO, msg, strlen(msg)); // terminal
		}
	}

	sprintf(msg, "\n..............................................................................................\n\n");
	write(STDOUT_FILENO, msg, strlen(msg)); // terminal
}

pid_t r_wait(int *stat_loc) {
	int retval;

	while (((retval = wait(stat_loc)) == -1) && (errno == EINTR));

	return retval;
}

void cleanup() {
	char msg[50];
	int i;
	int status;     // status from r_wait()
	remove_sem();
	remove_shared_memory();

	for (i = 0; i < number_of_procs;  i++) {
		kill(pids[i], SIGKILL);
		r_wait(&status);
		sprintf(msg, "Child %02d returned %d\n", i, WEXITSTATUS(status));
		writelog(msg);                            // logfile
	}

	system("./clean.sh");
	raise(SIGTERM);
}

/*shared memory and semaphore functions */
void shared_mem_init() {
	size_t shm_size_logical_clock = sizeof(logical_clock_t);

	if ((shm_clock_id = shmget(global_key, shm_size_logical_clock, SEM_FLAGS)) == -1) {
		perror("shmget: shm_clock_id failed");
		exit(EXIT_FAILURE);
	}

	size_t sem_size_logical_clock = sizeof(sem_t);

	if ((sem_clock_id = shmget(global_key, sem_size_logical_clock, SEM_FLAGS)) == -1) {
		perror("shmget: sem_clock_id failed");
		exit(EXIT_FAILURE);
	}

	size_t shm_size_page_table = sizeof(page_table_t);

	if ((shm_page_table_id = shmget(global_key, shm_size_page_table, SEM_FLAGS)) == -1) {
		perror("shmget: shm_page_table_id failed");
		exit(EXIT_FAILURE);
	}

	size_t sem_size_page_table = sizeof(sem_t);

	if ((sem_page_table_id = shmget(global_key, sem_size_page_table, SEM_FLAGS)) == -1) {
		perror("shmget: sem_page_table_id failed");
		exit(EXIT_FAILURE);
	}

	size_t shm_size_memory = sizeof(mem_reference_t);

	if ((mem_ref_id = shmget(global_key, shm_size_memory, SEM_FLAGS)) == -1) {
		perror("shmget: mem_ref_id failed");
		exit(EXIT_FAILURE);
	}

	size_t sem_size_memory = sizeof(sem_t);

	if ((sem_ref_id = shmget(global_key, sem_size_memory, SEM_FLAGS)) == -1) {
		perror("shmget: sem_ref_id failed");
		exit(EXIT_FAILURE);
	}

	size_t shm_size_page = sizeof(user_page_t);

	if ((page_id = shmget(global_key, shm_size_page, SEM_FLAGS)) == -1) {
		perror("shmget: page_id failed");
		exit(EXIT_FAILURE);
	}

	size_t sem_size_oss = sizeof(sem_t);

	if ((sem_oss_id = shmget(global_key, sem_size_oss, SEM_FLAGS)) == -1) {
		perror("shmget: sem_oss_id failed");
		exit(EXIT_FAILURE);
	}
}

void shared_mem_attach() {
	if ((logical_clock = (logical_clock_t *)shmat(shm_clock_id, NULL,  0)) == (void *) - 1) {
		perror("shmat: logical_clock attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((clock_sem = (sem_t *)shmat(sem_clock_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: clock_sem attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((page_table = (page_table_t *)shmat(shm_page_table_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: page_table attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((page_table_sem = (sem_t *)shmat(sem_page_table_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: page_table_sem attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((mem_reference = (mem_reference_t *)shmat(mem_ref_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: mem_reference attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((mem_ref_sem = (sem_t *)shmat(sem_ref_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: mem_ref_sem attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((page = (user_page_t *)shmat(page_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: page attachment failed");
		exit(EXIT_FAILURE);
	}

	if ((oss_sem = (sem_t *)shmat(sem_oss_id, NULL, 0)) == (void *) - 1) {
		perror("shmat: oss_sem attachment failed");
		exit(EXIT_FAILURE);
	}
}

void remove_shared_memory() {
	if (shmctl(shm_clock_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: shm_clock_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(sem_clock_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: sem_clock_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(shm_page_table_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: shm_page_table_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(sem_page_table_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: sem_page_table_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(mem_ref_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: mem_ref_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(sem_ref_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: sem_ref_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(page_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: page_id removal failed");
		exit(EXIT_FAILURE);
	}

	if (shmctl(sem_oss_id, IPC_RMID, NULL) == -1) {
		perror("shmctl: sem_oss_id removal failed");
		exit(EXIT_FAILURE);
	}
}

void init_sem() {
	if (sem_init(clock_sem, 1, 1) == -1) {
		perror("sem_init: Failed to initialize clock_sem");
		exit(EXIT_FAILURE);
	}

	if (sem_init(mem_ref_sem, 1, 1) == -1) {
		perror("sem_init: Failed to initialize mem_ref_sem");
		exit(EXIT_FAILURE);
	}

	if (sem_init(page_table_sem, 1, 1) == -1) {
		perror("sem_init: Failed to initialize page_table_sem");
		exit(EXIT_FAILURE);
	}

	if (sem_init(oss_sem, 1, 0) == -1) {
		perror("sem_init: Failed to initialize oss_sem");
		exit(EXIT_FAILURE);
	}
}

void remove_sem() {
	if (sem_destroy(clock_sem) == -1) {
		perror("sem_destroy: Failed to destroy clock_sem");
		exit(EXIT_FAILURE);
	}

	if (sem_destroy(mem_ref_sem) == -1) {
		perror("sem_destroy: Failed to destroy mem_ref_sem");
		exit(EXIT_FAILURE);
	}

	if (sem_destroy(page_table_sem) == -1) {
		perror("sem_destroy: Failed to destroy page_table_sem");
		exit(EXIT_FAILURE);
	}

	if (sem_destroy(oss_sem) == -1) {
		perror("sem_destroy: Failed to destroy oss_sem");
		exit(EXIT_FAILURE);
	}
}
