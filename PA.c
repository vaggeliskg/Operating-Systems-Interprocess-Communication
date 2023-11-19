#define TEXT_SZ 2048

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>

struct shared_use_st {
	char some_text_for_PA[TEXT_SZ];
    char some_text_for_PB[TEXT_SZ];
    char *local_buffer_PA;
    char *local_buffer_PB;
    int running;
    sem_t semA;
    sem_t semB;
};


void* sender(void* args) {
    //sem_wait(&sem);
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
    printf("Write something for sender of PA: ");
    fgets(shared_stuff->local_buffer_PA, BUFSIZ/2, stdin);
    strncpy(shared_stuff->some_text_for_PA, shared_stuff->local_buffer_PA, TEXT_SZ);
    if (strncmp(shared_stuff->local_buffer_PA, "end", 3) == 0) {
        shared_stuff->running = 0;
    }
    sem_post(&shared_stuff->semA);
    pthread_exit(NULL);
}

void* receiver(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
    sem_wait(&shared_stuff->semB);
    strncpy(shared_stuff->local_buffer_PA, shared_stuff->some_text_for_PB, TEXT_SZ); //might need to change later
    if(strlen(shared_stuff->local_buffer_PA) !=0 ) {
        printf("PB wrote: %s\n",shared_stuff->local_buffer_PA);
    }
    //sem_post(&shared_stuff->semA);
    pthread_exit(NULL);
 }

int main() { 
    int i;
	int running = 1;
    char buffer[BUFSIZ/2];
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
	int shmid;
	shmid = shmget((key_t)12345678, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
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

    pthread_t thread0,thread1;
	shared_stuff = (struct shared_use_st *)shared_memory;
    sem_init(&shared_stuff->semA,1,0);
    sem_init(&shared_stuff->semB,1,0);

    shared_stuff->local_buffer_PA = buffer;
    shared_stuff->running = running;

	while(running) {
        if( (pthread_create(&thread0, NULL, &sender, (void *)shared_stuff)) !=0) {
            perror("failed to create sender thread\n");
        }
        if( (pthread_create(&thread1, NULL, &receiver, (void *)shared_stuff)) !=0) {
            perror("failed to create receiver thread\n");
        }
        if( pthread_join(thread0, NULL) != 0) {
            perror("failed to join thread\n");
        }
        if( pthread_join(thread1, NULL) != 0) {
            perror("failed to join thread\n");
        }
        if(shared_stuff->running == 0) {
            running = 0;
        }
	}
	if (shmdt(shared_memory) == -1) {
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);    
 }
   