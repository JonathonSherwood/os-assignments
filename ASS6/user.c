/* JON SHERWOOD - CS 4760 - ASSIGNMENT 6 - MEMORY MANAGEMENT */

#include "shared.h"

void signal_init();
void signal_handler();
void shared_mem_attach();
void detach_shared_memory();
void pages_init();

user_page_t user_pages[NUM_PAGES];
sem_t *oss_sem;
mem_reference_t *mem_reference;
page_table_t *page_table;
user_page_t *page;
sem_t *clock_sem;
sem_t *mem_ref_sem;
sem_t *page_table_sem;

char msg[50];
int shm_clock_id;
int shm_page_table_id;
int mem_ref_id;
int page_id;
int sem_clock_id;
int sem_ref_id;
int sem_page_table_id;
int sem_oss_id;
int process_index;

logical_clock_t *logical_clock;


int main(int argc, char *argv[]) {
	process_index = atoi(argv[1]);
	pids[process_index] = getpid();
	sprintf(msg, "user: #%2d pid: %d running\n", process_index,  pids[process_index]);
	writelog(msg);
	write(STDOUT_FILENO, msg, strlen(msg));
	shm_clock_id = atoi(argv[2]);
	sem_clock_id = atoi(argv[3]);
	shm_page_table_id = atoi(argv[4]);
	sem_page_table_id = atoi(argv[5]);
	mem_ref_id = atoi(argv[6]);
	sem_ref_id = atoi(argv[7]);
	page_id = atoi(argv[8]);
	sem_oss_id = atoi(argv[9]);
	// signal_init();
	shared_mem_attach();
	int i, base;

	/*init user pages*/
	for (i = 0; i < NUM_PAGES; i++) {
		user_pages[i].page_num = i;
		user_pages[i].child_index = process_index;
		user_pages[i].range[0] = base;
		user_pages[i].range[1] = base + 1023;
		base += 1024;
	}

	/*for seed srand*/
	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);
	/*At random times, say every 1000 ± 100 memory references, the user process
	will check whether it should terminate.  If so, all its memory should be
	returned to oss and oss should be informed of its termination.*/
	int terminate_user = 1;

	while (terminate_user) {
		int max_user_ref_count = (rand() % 201) + 900;

		for (i = 1; i < max_user_ref_count; i++) {
			if (sem_wait(clock_sem) == -1) {
				perror("sem_wait: Failed to wait on clock_sem");
				exit(EXIT_FAILURE);
			}

			unsigned long ncount = i * 1000000;
			logical_clock->nsec += ncount;

			if (logical_clock->nsec >= 1000000000) {
				logical_clock->sec++;
				logical_clock->nsec -= 1000000000;
			}

			if (sem_post(clock_sem) == -1) {
				perror("sem_post: Failed to wait on clock_sem");
				exit(EXIT_FAILURE);
			}

			if (sem_post(mem_ref_sem) == -1) {
				perror("sem_post: Failed to signal mem_ref_sem");
				exit(EXIT_FAILURE);
			}

			mem_reference->rw = rand() % 2; // random read write
			int reference_page = mem_reference->address = (rand() % NUM_PAGES);
			mem_reference->pid = getpid();
			mem_reference->child_index = process_index;

			//prepare the page to copy on next page fault
			page->page_num = user_pages[reference_page].page_num;
			page->child_index = user_pages[reference_page].child_index;
			page->range[0] = user_pages[reference_page].range[0];
			page->range[1] = user_pages[reference_page].range[1];

			if (kill(getppid(), SIGUSR1) == -1) {
				perror("kill: Failed to send SIGUSR1 to OSS");
				exit(EXIT_FAILURE);
			}

			/*user process will wait on its semaphore that will be signaled by oss*/
			if (sem_wait(oss_sem) == -1) {
				perror("sem_wait: Failed to wait on oss_sem");
				exit(EXIT_FAILURE);
			}


			if (sem_post(mem_ref_sem) == -1) {
				perror("sem_post: Failed to signal mem_ref_sem");
				exit(EXIT_FAILURE);
			}

		}

		terminate_user = rand() % 2;
	}

	float ms = (((float)logical_clock->sec * 1000000000.0) + ((float)logical_clock->nsec)) / 1000000.0;
	sprintf(msg, "user: #%d terminating...[logical clock: %8.2f ms]\n", getpid(), ms);
	writelog(msg);                            // logfile
	write(STDOUT_FILENO, msg, strlen(msg));   // terminal
	detach_shared_memory();
	return 0;
}
void shared_mem_attach() {
	if ((logical_clock = (logical_clock_t *)shmat(shm_clock_id, NULL, 0)) == (void *) - 1) {
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

void detach_shared_memory() {
	if (shmdt(logical_clock) == -1) {
		perror("shmdt: logical_clock detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(clock_sem) == -1) {
		perror("shmdt: clock_sem detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(page_table) == -1) {
		perror("shmdt: page_table detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(page_table_sem) == -1) {
		perror("shmdt: page_table_sem detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(mem_reference) == -1) {
		perror("shmdt: mem_reference detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(mem_ref_sem) == -1) {
		perror("shmdt: mem_ref_sem detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(page) == -1) {
		perror("shmdt: page detach failed");
		exit(EXIT_FAILURE);
	}

	if (shmdt(oss_sem) == -1) {
		perror("shmdt: oss_sem detach failed");
		exit(EXIT_FAILURE);
	}
}
