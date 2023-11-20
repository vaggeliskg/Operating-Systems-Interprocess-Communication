#define TEXT_SZ 2048

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>

struct shared_use_st {
	int A;
    int B;
	char some_text_for_PA[TEXT_SZ];
    char some_text_for_PB[TEXT_SZ];
    char *local_buffer_PA;
    char *local_buffer_PB;
    int running;
	sem_t semA_test;
	sem_t semB_test;
};



void* sender(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
    printf("Write something for sender of PB: ");
    fgets(shared_stuff->local_buffer_PB, BUFSIZ/2, stdin);
    strncpy(shared_stuff->some_text_for_PB, shared_stuff->local_buffer_PB, TEXT_SZ);
    if(strncmp(shared_stuff->local_buffer_PB, "end", 3) == 0) {
        shared_stuff->running = 0;
    }
	shared_stuff->B = 1;
	sem_post(&shared_stuff->semA_test);
	sem_post(&shared_stuff->semB_test);
}

void* receiver(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
	sem_wait(&shared_stuff->semB_test);
	if(shared_stuff->B == 1) {
		//sem_wait(&shared_stuff->semA_test);
		shared_stuff->B = 0;
        pthread_exit("leave");
    }
    strncpy(shared_stuff->local_buffer_PB, shared_stuff->some_text_for_PA, TEXT_SZ); //might need to change later
   	if(strlen(shared_stuff->local_buffer_PB) !=0 ) {
        printf("\nPA wrote: %s\n",shared_stuff->local_buffer_PB);
    }
	//sem_post(&shared_stuff->semB);
}

int main() {
	int running = 1;
	int i;
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
	char buffer[BUFSIZ/2];
	int shmid;
	shmid = shmget((key_t)123456787, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
	//printf("Shared memory segment with id %d attached at %p\n", shmid, shared_memory);

	shared_stuff = (struct shared_use_st *)shared_memory;

	shared_stuff->A = 0;
    shared_stuff->B = 0;
	// sem_init(&shared_stuff->semA_test,1,0);
	sem_init(&shared_stuff->semB_test,1,0);
    pthread_t thread0, thread1;

	shared_stuff->local_buffer_PB = buffer;
	shared_stuff->running = running;
	while(running) {
		if( (pthread_create(&thread1, NULL, &receiver, (void *)shared_stuff)) !=0) {
        	perror("failed to create receiver thread\n");
        }
        if( (pthread_create(&thread0, NULL, &sender, (void *)shared_stuff)) !=0) {
            perror("failed to create sender thread\n");
        }
        if( pthread_join(thread1, NULL) != 0) {
            perror("failed to join thread\n");
        }
		if(shared_stuff->running == 0) {
            running = 0;
			break;
        }
        if( pthread_join(thread0, NULL) != 0) {
            perror("failed to join thread\n");
        }
	}
	if (shmdt(shared_memory) == -1) {
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		fprintf(stderr, "shmctl(IPC_RMID) failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
