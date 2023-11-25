#define TEXT_SZ 2048
#define PACKET_SIZE 15

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

//Struct used in the shared memory segment
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
    int number_of_A_packets;
    int number_of_B_packets;
    long int time_for_A;
    long int time_for_B;
    struct timeval tv_A;
    struct timeval tv_B;
    int first_packet_A;
    int first_packet_B;
};

//Thread responsible for sending messages to PB
void* sender(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
    while(shared_stuff->running) {
        printf("Write something for sender of PA: ");
        fgets(shared_stuff->local_buffer_PA, BUFSIZ/2, stdin);
        int length = strlen(shared_stuff->local_buffer_PA);     //Calculate size of message
        int i;
        if(length > PACKET_SIZE) {                              //If size > 15 then you need to divide it and send individual packets
            for (i = 0; i < length; i += PACKET_SIZE) {
                char packet[PACKET_SIZE + 1];                   // Packet size + 1 for null terminator
                strncpy(packet, shared_stuff->local_buffer_PA + i, PACKET_SIZE);
                packet[PACKET_SIZE] = '\0';                     // Null-terminate the packet
                strncpy(shared_stuff->some_text_for_PA, packet, PACKET_SIZE);
                sem_post(&shared_stuff->semB_test);             //Semaphore enabling the PB receiver to process the message
                shared_stuff->A = 1;                      
                shared_stuff->message_via_packets_A = 1;
                shared_stuff->number_of_A_packets++;
                if(i == 0) {
                    gettimeofday(&shared_stuff->tv_A, NULL);   //If its the first packet then calculate the time it was sent
                    shared_stuff->first_packet_A = 1;
                }
                sem_wait(&shared_stuff->packet_sent_from_A);  //Wait the PB receiver before you send the next packet
            }
            shared_stuff->number_of_A_messages++;             //Increase the number of messages when you are done sending the packets
        }
        //If the message is < 15 characters just send it , still increasing the packet numbers
        else {
            strncpy(shared_stuff->some_text_for_PA, shared_stuff->local_buffer_PA, TEXT_SZ);
            sem_post(&shared_stuff->semB_test);
            shared_stuff->number_of_A_messages++;
            shared_stuff->A = 1;
            shared_stuff->number_of_A_packets++;
            gettimeofday(&shared_stuff->tv_A, NULL);
        }
        //If #BYE# is typed , terminate by switching running to 0
        if (strncmp(shared_stuff->local_buffer_PA, "#BYE#", 5) == 0) {
            shared_stuff->running = 0;
            shared_stuff->A = 1;
            shared_stuff->number_of_A_messages--;
            shared_stuff->number_of_A_packets--;
            sem_post(&shared_stuff->semA_test);
        }
    }
}

void* receiver(void* args) {
    struct shared_use_st *shared_stuff = (struct shared_use_st *)args;
    struct timeval tv_receive_A;
    while(shared_stuff->running) {
        int full_message = 0;
        sem_wait(&shared_stuff->semA_test);             //Semaphore waiting to receive a message
        if(!shared_stuff->running) {
            break;
        } 
        if(shared_stuff->A == 1) {                      //If PA was the last to write (A == 1) make A = 0
            shared_stuff->local_buffer_PA[0] = '\0';    //and empty the local buffer 
            shared_stuff->A = 0;
        }
        //If the message was < 15 characters and it's not part of a message that was sent via packets
        if(strlen(shared_stuff->some_text_for_PB) <= PACKET_SIZE && shared_stuff->messages_via_packets_B == 0) {
            strncpy(shared_stuff->local_buffer_PA, shared_stuff->some_text_for_PB, TEXT_SZ);
			if(strlen(shared_stuff->local_buffer_PA) !=0 ) {
				printf("\nPB wrote: %s\n",shared_stuff->local_buffer_PA);
                shared_stuff->local_buffer_PA[0] = '\0';
                gettimeofday(&tv_receive_A, NULL);
			}
		}
        //Else if its part of a message that was sent via packets
        else {
            sem_post(&shared_stuff->packet_sent_from_B); //Process each packet
            if(shared_stuff->first_packet_B == 1) {      //Calculate time of day if it's the first packet
                gettimeofday(&tv_receive_A, NULL);
                shared_stuff->first_packet_B = 0;
            }
            strcat(shared_stuff->local_buffer_PA, shared_stuff->some_text_for_PB);  //add the packet right next to the previous packet
            if (strlen(shared_stuff->some_text_for_PB) < PACKET_SIZE) {             // main reason why shared_stuff->local_buffer_PA[0] = '\0'; is needed
                full_message = 1;
            }
            if(full_message) {                                                      //All the packets have been received
                if(strlen(shared_stuff->local_buffer_PA) !=0 ) {          
                    printf("\nPB wrote: %s\n",shared_stuff->local_buffer_PA);
                    shared_stuff->local_buffer_PA[0] = '\0';
                    shared_stuff->messages_via_packets_B = 0;
                }
            }
        }
        shared_stuff->time_for_A += (tv_receive_A.tv_usec - shared_stuff->tv_B.tv_usec); //Calculate the time between packets deliveries
    }
 }

int main() { 
    int i;
	int running = 1;
    char buffer[BUFSIZ/2];
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
	int shmid;
    //Create shared memory
	shmid = shmget((key_t)9, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
    //Create threads, initialize semaphores, and necessary shared_stuff members vars
    pthread_t thread0,thread1;
	shared_stuff = (struct shared_use_st *)shared_memory;
    sem_init(&shared_stuff->semA_test,1,0);
    sem_init(&shared_stuff->packet_sent_from_A,1,0);

    shared_stuff->local_buffer_PA = buffer;
    shared_stuff->running = running;
    shared_stuff->A = 0;
    shared_stuff->B = 0;
    shared_stuff->message_via_packets_A = 0;
    shared_stuff->number_of_A_messages = 0;
    shared_stuff->number_of_A_packets = 0;
    shared_stuff->first_packet_A = 0;
    if( (pthread_create(&thread0, NULL, &sender, (void *)shared_stuff)) !=0) {
        perror("failed to create sender thread\n");
    }
    if( (pthread_create(&thread1, NULL, &receiver, (void *)shared_stuff)) !=0) {
        perror("failed to create receiver thread\n");
    }
    if( pthread_join(thread1, NULL) != 0) {
        perror("failed to join thread\n");
    }
    //If the other process ended the interaction cancel the thread , dont wait for the sender of this process
    if(shared_stuff->running == 0 && shared_stuff->B == 1){
        pthread_cancel(thread0);
    }
    if( pthread_join(thread0, NULL) != 0) {
        perror("failed to join thread\n");
    }
    //Stats calculations
    int average_packets = 0;
    if(shared_stuff->number_of_A_messages != 0) {
        average_packets = shared_stuff->number_of_A_packets / shared_stuff->number_of_A_messages;
    }
    int average_time = 0;
    if(shared_stuff->number_of_B_messages != 0) {
        average_time = shared_stuff->time_for_A / shared_stuff->number_of_B_messages;
    }
    printf("\n");
    printf("A sent: %d messages\n",shared_stuff->number_of_A_messages);
    printf("A received: %d messages\n",shared_stuff->number_of_B_messages);
    printf("A sent: %d packets\n",shared_stuff->number_of_A_packets);
    printf("Average packets per message for A: %d\n",average_packets);
    printf("Average time for A: %d \n ",average_time);

    if (shmdt(shared_memory) == -1) {
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);    
 }
   