/* John An and Edward Schembor
 * Distributed Systems
 * Exercise 2
 * mcast program
 */

#include <arpa/inet.h>
#include <string.h>
#include "net_include.h"
#include "token.h"
#include "packet.h"
#include "ip_packet.h"
#include "recv_dbg.h"
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define HOLDING_SIZE 100
#define FRAME_SIZE 250
#define NAME_LENGTH 80

#define USED_CLOCK CLOCK_MONOTONIC /* CLOCK_MONOTONIC_RAW if available*/
#define NANOS 1000000000LL

void recv_dbg_init(int percent, int machine_index);
void final_report();
void write_packet(packet *p);
int minimum(int a, int b);
void retransmit();
void fill_retrans();
void fill_frame();

struct sockaddr_in name;
struct sockaddr_in send_addr;
struct sockaddr_in my_addr; /*Address for this machine*/
struct sockaddr_in neighbor; /*Address for the machine after*/
struct timeval     start, end;

struct timespec    begin, current;
long long          startt, elapsed, microseconds;

struct hostent     h_ent;
struct hostent     *p_h_ent;

int                my_ip;
int                mcast_addr;

int                seen_token = 0;
int                has_token = 0;
int                has_neighbor = 0;
int                sent_token = 0;
int                packet_type;

char               machine_name[NAME_LENGTH] = {'\0'};

struct ip_mreq     mreq;
unsigned char      ttl_val;

int                ss,sr,i,j;
fd_set             mask;
fd_set             dummy_mask,temp_mask;
int                bytes;
int                num;
char               mess_buf[MAX_MESS_LEN];
char               input_buf[80];
struct timeval     timeout;

packet             *received_packet;
int                num_packets;
int                machine_index;
int                num_machines;
int                loss_rate;

token              tkn;
ip_packet          *i_packet;
packet             *buffer;
packet             *machine_finished; /*Tells other machines this is done*/
packet             *all_machines_finished; /*Tells others all are done*/

int                local_aru = 0;
int                sent_packets = 0;
int                last_seen_aru = 0;
packet             *holding[HOLDING_SIZE];
packet             *frame[FRAME_SIZE];
FILE               *file;
int                highest_received = 0;
int                num_finished = 0;
int                all_have = -1; /*Index which all machines have received*/
int                last_all_have = -1; /*The value of the last all_have*/
int                local_round = 0; /* local round */
int                last_seq = 0;

int main(int argc, char **argv)
{

    /*Make sure the usage is correct*/
	if(argc != 5) {
        printf("Usage: mcast <num_packets> <machine_index> <num_of_machines> <loss_rate>\n");
		exit(0);
	}

	/*Get the start time*/
	gettimeofday(&start, NULL);

	/*Set variables from input*/
    num_packets = atoi(argv[1]);
	machine_index = atoi(argv[2]);
	num_machines = atoi(argv[3]);
	loss_rate = atoi(argv[4]);

	/*Open file for writing*/
	char filename[NAME_LENGTH];
	sprintf(filename, "%d", machine_index);
	strcat(filename, ".out");
	printf("%s", filename);
	if ((file = fopen(filename, "w")) == NULL) {
		perror("fopen");
		exit(0);
	}

    /*Initialize loss rate*/
    recv_dbg_init(loss_rate, machine_index);

	/*Seed random number generator for future use*/
	srand(time(NULL));

    /*Malloc all needed memory*/
	received_packet = malloc(sizeof(packet));
	i_packet = malloc(sizeof(ip_packet));
    buffer = malloc(PACKET_SIZE);
	for(i = 0; i < FRAME_SIZE; i++) {
		frame[i] = malloc(sizeof(packet));
        frame[i]->packet_index = -1;
	}
	for(i = 0; i < HOLDING_SIZE; i++) {
		holding[i] = malloc(sizeof(packet));
	}
	all_machines_finished = malloc(sizeof(packet));

	/*Creates the mcast address*/
    mcast_addr = 225 << 24 | 1 << 16 | 2 << 8 | 103; /* (225.1.2.103) */

    sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving */
    if (sr<0) {
        perror("Mcast: socket");
        exit(1);
    }

	/*Set my_address struct to send to other processes*/
    gethostname(machine_name, NAME_LENGTH);
	p_h_ent = gethostbyname(machine_name);
	if(p_h_ent == NULL) {
        printf("mcast: gethostbyname error.\n");
		exit(1);
	}
	memcpy( &h_ent, p_h_ent, sizeof(h_ent));
	memcpy( &my_ip, h_ent.h_addr_list[0], sizeof(my_ip));

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = my_ip;
    my_addr.sin_port = htons(PORT);


    /*Used to set up receiving socket*/
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    /*Receiving socket bind*/
    if ( bind( sr, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
        perror("Mcast: bind");
        exit(1);
    }

    /*WILL ASK ABOUT FOLLOWING  3 LINES*/
    mreq.imr_multiaddr.s_addr = htonl( mcast_addr );
    mreq.imr_interface.s_addr = htonl( INADDR_ANY );

	/*Joins receiving socket to the multicast address*/
    if (setsockopt(sr, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, 
        sizeof(mreq)) < 0) 
    {
        perror("Mcast: problem in setsockopt to join multicast address" );
    }

    ss = socket(AF_INET, SOCK_DGRAM, 0); /* Socket for sending */
    if (ss<0) {
        perror("Mcast: socket");
        exit(1);
    }

    ttl_val = 1;
    if (setsockopt(ss, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&ttl_val, 
        sizeof(ttl_val)) < 0) 
    {
        printf("Mcast: problem in setsockopt of multicast ttl %d - ignore in WinNT or Win95\n", ttl_val );
    }

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = htonl(mcast_addr);  /* mcast address */
    send_addr.sin_port = htons(PORT);

    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sr, &mask );
    FD_SET( (long)0, &mask );    /* stdin */

    /***********************************************
     * *********************************************
     * THE LOGIC STARTS HERE ***********************
     * *********************************************
     * ********************************************/

    /*Wait for the special start packet from start_mcast*/
	for(;;)
	{
        temp_mask = mask;
	    num = select( FD_SETSIZE, &temp_mask, &dummy_mask,&dummy_mask, NULL);
	    bytes = recv( sr, (char *) received_packet, PACKET_SIZE, 0 );
		printf("bytes = %d\n", bytes);
		if(received_packet->machine_index == -1) {
            printf("\n%d\n", received_packet->machine_index);
            break;
		}
	}

/************************************
 * **********************************
 * CREATE TOKEN AND OWN IP PACKET BEFORE MOVING ON
 * **********************************
 * *********************************/

	/*The first process creates the initial token*/
    if(machine_index == 1) {
        for(i = 0; i < RETRANS_SIZE; i++) {
            tkn.retrans_req[i] = -1;
		}
        tkn.type = 1;
		tkn.sequence = 0;
		tkn.aru = -1;
        has_token = 1;
		tkn.round = 1;
        if (clock_gettime(USED_CLOCK, &begin)) {
            /* Getting clock time failed */
            exit(EXIT_FAILURE);
        }
        startt = begin.tv_sec*NANOS + begin.tv_nsec;
	}

    /*Connect this process to the next process*/
    i_packet->machine_index = machine_index;
	i_packet->addr = my_addr;
    i_packet->type = 2;

    printf("\nMy adress is: %s\n", inet_ntoa(my_addr.sin_addr));

    printf("\nI'm machine number: %d\n", machine_index);

/********************************
 * ******************************
 * IDENTIFY NEIGHBORS
 * ******************************
 * ******************************/

    for(;;)
    {
        temp_mask = mask;

		/*Check if process 1 timed out, which means the token was lost*/
		if(machine_index == 1) {
            if (clock_gettime(USED_CLOCK, &current)) {
                /* getting clock time failed, what now? */
                exit(EXIT_FAILURE);
            }
            elapsed = current.tv_sec*NANOS + current.tv_nsec - startt;
            microseconds = elapsed / 1000 + (elapsed % 1000 >= 500); /* round up halves*/
			if(microseconds > 100000) {
				has_token = 1;
                if (clock_gettime(USED_CLOCK, &begin)) {
                    /* Oops, getting clock time failed */
                    exit(EXIT_FAILURE);
                }
                startt = begin.tv_sec*NANOS + begin.tv_nsec;
				printf("\nPROCESS 1 TIMED OUT*************\n");
			}
		}

        /*If I've seen the token and I've sent it successfully, I end*/
        if (seen_token && has_neighbor) {
            break;
        }

		timeout.tv_usec = 50000;
		num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);

        if (num > 0) {
            /** receive packet **/
            bytes = recv_dbg( sr, (char *) buffer, PACKET_SIZE, 0 );
			packet_type = buffer->type;

            /** If token **/
            if (packet_type == 1) {
				printf("\nI got the token!\n");
			    tkn = *((token *)buffer);
				has_token = 1;
				seen_token = 1;
				if(machine_index == 1) {
					tkn.is_connected = 1;
					break;
				}

            }  else if (packet_type == 2) {

                /** Check that the ip_packet's index is your neighbor**/
                if ( !has_neighbor && ((ip_packet *) buffer)->machine_index == ((machine_index % num_machines) + 1)) {
                    /** Set the ip_packet's address to your neighbor**/
					printf("\nI got my neighbor\n");
                    neighbor = ((ip_packet *) buffer) -> addr;
                    has_neighbor = 1;
                }
            }
        } else if(num == 0) {
			sendto( ss, i_packet, sizeof(ip_packet), 0, 
                (struct sockaddr *)&send_addr, sizeof(send_addr) );
			printf("\nI timed out!\n");
		}
        /** If you have a token and your neighbor, send the token **/
        if (has_token && has_neighbor) {
			printf("\nI sent the token to%s\n", inet_ntoa(neighbor.sin_addr));
            sendto( ss, &tkn, sizeof(token), 0, 
				(struct sockaddr *)&neighbor, sizeof(neighbor) );
			has_token = 0;
        }
    }

    printf("\n----------------------I BROKE OUT--------------------\n");
	if(has_token) {
		printf("\n---------------------I have the token------------------\n");
        tkn.is_connected = 1;
 		sendto( ss, &tkn, sizeof(token), 0, 
			(struct sockaddr *)&neighbor, sizeof(neighbor) );
		has_token = 0;
		printf("\n\n------------token: %d----------------------\n\n", tkn.is_connected);


	}


/**************************
 * ************************
 * SINGLE RING TOKEN BEGINS HERE
 * ************************    
 * ***********************/
	

	for(;;) {

		temp_mask = mask;
		num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask,
			&timeout);
		//printf("\nSelected\n");
		if(num > 0) {
			bytes = recv_dbg(sr, (char *) buffer, PACKET_SIZE, 0);
			packet_type = buffer->type;
			//printf("\nReceived\n");

			/** We receive a token**/
			if(packet_type == 1) {
				tkn = *((token *)buffer);
				//printf("\nlocal round: %d\n", local_round);
				printf("\ntkn round: %d\n", tkn.round);
				printf("\ntkn is connected %d\n", tkn.is_connected);
				if (local_round < tkn.round /*&& tkn.is_connected*/) {
					if((all_have == tkn.sequence) && (tkn.sequence == last_seq)) {
						all_machines_finished->type = 3;
						sendto(ss, all_machines_finished, sizeof(packet), 0,
						(struct sockaddr *)&send_addr, sizeof(send_addr));
						break;
					}
					printf("\nPre-retrans\n");
					retransmit();
					printf("\nRetransmitted\n");
					fill_retrans();
					printf("\nFilled retrans\n");
					fill_frame();
					printf("\nFilled frame\n");
					if (local_aru < tkn.aru || machine_index == tkn.last_lowered) {
						tkn.aru = local_aru;
						tkn.last_lowered = machine_index;
					}
					if (machine_index == 1) {
						tkn.round++;
					}
					local_round++;
					last_seq = tkn.sequence;
				}
				sendto(ss, &tkn, sizeof(token), 0, 
					(struct sockaddr *)&neighbor, sizeof(neighbor));
				has_token = 0;
				//printf("\nSent token\n");

			/** If we receive a data packet **/		
			} else if (packet_type == 0) {
				
				/** If the packet's index > local ARU, add to holding 
				 * array **/
				if ((buffer->packet_index > local_aru) && !(buffer->packet_index > FRAME_SIZE+local_aru)) {
					holding[buffer->packet_index % FRAME_SIZE] = buffer;
					printf("\nGot packet w/ index: %d\n", buffer->packet_index);
					if(buffer->packet_index > highest_received) {
						highest_received = buffer->packet_index;
					}
				}

				/** First packet in holding array edge case, write 
				 * and update **/
				if (local_aru == -1 && holding[0]->packet_index != -1) {
					local_aru++;
					write_packet(holding[0]);
					
				}

				/** Write all packets you can **/
				while(local_aru < holding[(local_aru+1) % HOLDING_SIZE]->packet_index) {
					write_packet(holding[local_aru % HOLDING_SIZE]);
					local_aru++;
				}

			/** If we receive a termination packet **/
			} else if (packet_type == 3) {
				all_machines_finished->type = 3;
				sendto(ss, all_machines_finished, sizeof(packet), 0,
					(struct sockaddr *)&send_addr, sizeof(send_addr));
				break;
			} else {
				printf("\npacket_type: %d\n", packet_type);
				printf("\nerror :( or other ip\n");
			}
		}
	}

	/** Free all malloc'd memory **/
	free(received_packet);
	free(i_packet);
	free(buffer);
	free(machine_finished);
	free(all_machines_finished);
	for(i = 0; i < FRAME_SIZE; i++) {
		free(frame[i]);
	}
	for(i = 0; i < HOLDING_SIZE; i++) {
		free(holding[i]);
	}

	fclose(file);
	final_report();
    return 0;

}

void final_report() {
	gettimeofday(&end, NULL);
	printf("\n*******PROCESS HAS ENDED**********");
	printf("\nTotal Time Taken: %lu \n", ((end.tv_sec*1000000 + end.tv_usec)
		- (start.tv_sec * 1000000 + start.tv_usec)));
}

void write_packet(packet *p) {
	fprintf(file, "%2d, %8d , %8d\n", p->machine_index, p->packet_index, 
		p->random_number);
}

int minimum(int a, int b) {
	if(a < b) {
		return a;
	}else{
		return b;
	}
}

void retransmit() {
	packet *temp_packet;
	for(int i = 0; i < RETRANS_SIZE; i++) {
		if(tkn.retrans_req[i] != -1) {
			temp_packet = holding[tkn.retrans_req[i] % HOLDING_SIZE];
			if (tkn.retrans_req[i] == temp_packet->packet_index){
				sendto(ss, temp_packet, sizeof(packet), 0, 
					(struct sockaddr *)&send_addr, sizeof(send_addr));
				tkn.retrans_req[i] = -1;
			}
		}
	}
}

void fill_retrans() {
	int temp = local_aru;
	for(i = 0; i < RETRANS_SIZE; i++) {
		if(tkn.retrans_req[i] == -1) {
			for(int j = temp; j < highest_received; j++) {
				if(holding[j%HOLDING_SIZE]->packet_index < local_aru) {
					tkn.retrans_req[i] = j;
					temp = ++j;
					break;
				}
			}
		}
	}
}

void fill_frame() {
	int temp = last_all_have + 1;
	printf("\ntemp: %d  index: %d  all_have: %d\n", temp, frame[temp]->packet_index, all_have);
	while(frame[temp % FRAME_SIZE]->packet_index <= all_have) {
		printf("\ntemp: %d  index: %d  all_have: %d\n", temp, frame[temp]->packet_index, all_have);
		printf("\nFilling: %d\n", temp);
		frame[temp % FRAME_SIZE]->random_number = rand();
		frame[temp % FRAME_SIZE]->type = 0;
		frame[temp % FRAME_SIZE]->machine_index = machine_index;
		frame[temp % FRAME_SIZE]->packet_index = tkn.sequence++;
		sent_packets++;
		temp++;
		sendto(ss, machine_finished, sizeof(packet), 0,
			(struct sockaddr *)&send_addr, sizeof(send_addr));
	}
}
