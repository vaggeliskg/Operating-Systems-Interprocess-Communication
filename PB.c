#define TEXT_SZ 2048
#define PACKET_SIZE 15

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
	int message_via_packets_A;
	int messages_via_packets_B;
	sem_t packet_sent_from_A;
    sem_t packet_sent_from_B;
	int number_of_A_messages;
    int number_of_B_messages;
	int number_of_B_packets;
};



void* sender(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
	while(shared_stuff->running) {
		printf("Write something for sender of PB: ");
		fgets(shared_stuff->local_buffer_PB, BUFSIZ/2, stdin);
		
		int length = strlen(shared_stuff->local_buffer_PB);
        int i;
		if(length > PACKET_SIZE) {
			for (i = 0; i < length; i += PACKET_SIZE) {
				char packet[PACKET_SIZE + 1]; // Packet size + 1 for null terminator
				strncpy(packet, shared_stuff->local_buffer_PB + i, PACKET_SIZE);
				packet[PACKET_SIZE] = '\0'; // Null-terminate the packet
				strncpy(shared_stuff->some_text_for_PB, packet, PACKET_SIZE);
				sem_post(&shared_stuff->semA_test);
				shared_stuff->B = 1;
				shared_stuff->messages_via_packets_B = 1;
				shared_stuff->number_of_B_packets++;
				sem_wait(&shared_stuff->packet_sent_from_B);
			}
			// if(shared_stuff->messages_via_packets_B = 1) {
				//shared_stuff->local_buffer_PB[0] = '\0';
				shared_stuff->number_of_B_messages++;
			// }
		}
		else {
            strncpy(shared_stuff->some_text_for_PB, shared_stuff->local_buffer_PB, TEXT_SZ);
			sem_post(&shared_stuff->semA_test);
			shared_stuff->number_of_B_messages++;
			shared_stuff->B = 1;
			//shared_stuff->local_buffer_PA[0] = '\0';
		}
		
		//strncpy(shared_stuff->some_text_for_PB, shared_stuff->local_buffer_PB, TEXT_SZ);
		if(strncmp(shared_stuff->local_buffer_PB, "end", 3) == 0) {
			shared_stuff->running = 0;
			shared_stuff->B = 1;
			shared_stuff->number_of_B_messages--;
			sem_post(&shared_stuff->semB_test);
		}
	}
}

void* receiver(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
	while(shared_stuff->running) {
		int full_message = 0;
		sem_wait(&shared_stuff->semB_test);
		if(!shared_stuff->running) {
			break;
		}
		if(shared_stuff->B == 1) {
			shared_stuff->local_buffer_PB[0] = '\0';
			shared_stuff->B = 0;
		}
		if(strlen(shared_stuff->some_text_for_PA) <= PACKET_SIZE && shared_stuff->message_via_packets_A == 0) {
			strncpy(shared_stuff->local_buffer_PB, shared_stuff->some_text_for_PA, TEXT_SZ);
			if(strlen(shared_stuff->local_buffer_PB) !=0 ) {
				printf("\nPA wrote: %s\n",shared_stuff->local_buffer_PB);
				shared_stuff->local_buffer_PB[0] = '\0';
			}
		}
		else {
			sem_post(&shared_stuff->packet_sent_from_A);
			strcat(shared_stuff->local_buffer_PB, shared_stuff->some_text_for_PA);
			if (strlen(shared_stuff->some_text_for_PA) < PACKET_SIZE) {
				full_message = 1;
			}
			if(full_message) {
				if(strlen(shared_stuff->local_buffer_PB) !=0 ) {
					printf("\nPA wrote: %s\n",shared_stuff->local_buffer_PB);
					shared_stuff->local_buffer_PB[0] = '\0';
					shared_stuff->message_via_packets_A = 0;
				}
			}
		}
	}
}

int main() {
	int running = 1;
	int i;
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
	char buffer[BUFSIZ/2];
	int shmid;
	shmid = shmget((key_t)112456581, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
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


	sem_init(&shared_stuff->semB_test,1,0);
    sem_init(&shared_stuff->packet_sent_from_B,1,0);

    pthread_t thread0, thread1;

	shared_stuff->local_buffer_PB = buffer;
	shared_stuff->running = running;
	shared_stuff->messages_via_packets_B = 0;
	shared_stuff->number_of_B_messages = 0;
	shared_stuff->number_of_B_packets = 0;
	if( (pthread_create(&thread0, NULL, &sender, (void *)shared_stuff)) !=0) {
		perror("failed to create sender thread\n");
	}
	if( (pthread_create(&thread1, NULL, &receiver, (void *)shared_stuff)) !=0) {
		perror("failed to create receiver thread\n");
	}
	if( pthread_join(thread1, NULL) != 0) {
		perror("failed to join thread\n");
	}
	if(shared_stuff->running == 0 && shared_stuff->A == 1){
        pthread_cancel(thread0);
    }
	if( pthread_join(thread0, NULL) != 0) {
		perror("failed to join thread\n");
	}
	printf("B sent: %d messages\n",shared_stuff->number_of_B_messages);
	printf("B received: %d messages\n",shared_stuff->number_of_A_messages);
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
