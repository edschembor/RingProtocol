/* John An and Edward Schembor
 * Distributed Systems
 * Exercise 2
 * mcast revised program
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

#define BUFFER_SIZE 5000
#define WINDOW_SIZE 2500
#define BURST_SIZE  150
#define NAME_LENGTH 80

#define USED_CLOCK CLOCK_MONOTONIC /* CLOCK_MONOTONIC_RAW if available*/
#define NANOS 1000000000LL

void recv_dbg_init(int percent, int machine_index);
void final_report();
void write_packets();
int minimum(int a, int b);
void retransmit();
void fill_retrans();
void fill_window();
//void set_local_aru();

struct sockaddr_in name;
struct sockaddr_in send_addr;
struct sockaddr_in my_addr; /*Address for this machine*/
struct sockaddr_in neighbor; /*Address for the machine after*/
struct timeval     start, end;

struct timespec    begin, current; //used for timeouts
long long          startt, elapsed, microseconds; //used for timeouts

struct hostent     h_ent;
struct hostent     *p_h_ent;

int                my_ip;
int                mcast_addr;

int                seen_token = 0;
int                has_token = 0;
int                has_neighbor = 0;
int                packet_type;

char               machine_name[NAME_LENGTH] = {'\0'};

struct ip_mreq     mreq;
unsigned char      ttl_val;

int                ss,sr,i,j;
fd_set             mask;
fd_set             dummy_mask,temp_mask;
int                bytes;
int                num;

struct timeval     timeout;

packet             *received_packet;

int                num_packets;
int                machine_index;
int                num_machines;
int                loss_rate;

token              tkn;
token              tkn_buf;
ip_packet          *i_packet;
packet             *buffer;
packet             *machine_finished; /*Tells other machines this is done*/
packet             *all_machines_finished; /*Tells others all are done*/
int                num_finished = 0;

int                local_aru = -1;
int                sent_packets = 0; /*Number of unique packets machine sent*/
int                last_seen_aru = -1;
packet             *recv_buf[BUFFER_SIZE]; /*Array for receiving*/
FILE               *file;
int                global_aru = -1; /*Index which all machines have received*/
int                local_round = 0; /* local round */
int                last_seq = -1;
int                beginning = 0; /*start of window*/

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

    char machines_finished[num_machines];
    for (int i = 0; i < num_machines; i++) {
        machines_finished[i] = 0;
    }
	/*Open file for writing*/
	char filename[NAME_LENGTH];
	sprintf(filename, "%d", machine_index);
	strcat(filename, ".out");
	if ((file = fopen(filename, "w")) == NULL) {
		perror("fopen");
		exit(0);
	}

    /*Initialize loss rate*/
    recv_dbg_init(loss_rate, machine_index);

	/*Seed random number generator for future use*/
	srand(time(NULL));

    /*Malloc all needed memory*/
	received_packet = malloc(PACKET_SIZE);
	i_packet = malloc(sizeof(ip_packet));
    buffer = malloc(PACKET_SIZE);
	for(i = 0; i < BUFFER_SIZE; i++) {
		recv_buf[i] = malloc(sizeof(packet));
		recv_buf[i]->packet_index = -1;
	}
	all_machines_finished = malloc(sizeof(packet));
    machine_finished = malloc(sizeof(packet));

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
		if(received_packet->machine_index == -1) {
            break;
		}
	}

	/*************************************************
 	* ***********************************************
 	* CREATE TOKEN AND OWN IP PACKET BEFORE MOVING ON
	* ***********************************************
 	* ***********************************************/

	/*The first process creates the initial token*/
    if(machine_index == 1) {
        for(i = 0; i < RETRANS_SIZE; i++) {
            tkn.retrans[i] = -1;
		}
        tkn.type = 1;
		tkn.sequence = -1;
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
			if(microseconds > 10000) {
				has_token = 1;
                if (clock_gettime(USED_CLOCK, &begin)) {
                    /* Oops, getting clock time failed */
                    exit(EXIT_FAILURE);
                }
                startt = begin.tv_sec*NANOS + begin.tv_nsec;
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

            /** If token Packet **/
            if (packet_type == 1) {
			    tkn = *((token *)buffer);
				has_token = 1;
				seen_token = 1;
				if(machine_index == 1) {
					tkn.is_connected = 1;
					break;
				}

			/** If IP Packet **/
            }  else if (packet_type == 2) {

                /** Check that the ip_packet's index is your neighbor**/
                if ( !has_neighbor && ((ip_packet *) buffer)->machine_index == ((machine_index % num_machines) + 1)) {
                    /** Set the ip_packet's address to your neighbor**/
                    neighbor = ((ip_packet *) buffer) -> addr;
                    has_neighbor = 1;
                }
            }
        } else if(num == 0) {
			sendto( ss, i_packet, sizeof(ip_packet), 0, 
                (struct sockaddr *)&send_addr, sizeof(send_addr) );
		}
        /** If you have a token and your neighbor, send the token **/
        if (has_token && has_neighbor) {
            sendto( ss, &tkn, sizeof(token), 0, 
				(struct sockaddr *)&neighbor, sizeof(neighbor) );
			has_token = 0;
        }
    }

	/** If you have the token, pass it - need this for ring token logic
	 *  to begin properly **/
	if(has_token) {
        tkn.is_connected = 1;
 		sendto( ss, &tkn, sizeof(token), 0, 
			(struct sockaddr *)&neighbor, sizeof(neighbor) );
		has_token = 0;
	}


/********************************
 * ******************************
 * SINGLE RING TOKEN BEGINS HERE
 * ******************************    
 * ******************************/
	


	for(;;) {

		temp_mask = mask;

        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask,
			&timeout);
		if(num > 0) {
			bytes = recv_dbg(sr, (char *) buffer, PACKET_SIZE, 0);
			packet_type = buffer->type;

			/** We receive a token**/
			if(packet_type == 1) { 
				/* Cast the received token to a buffer */
				tkn_buf = *((token *)buffer);
                
				/* If the token has made a complete rotation, increase its 
				 * curret round */
				if ((local_round == tkn_buf.round)&&(machine_index == 2)) {
                    tkn_buf.round++;
                }

				/* If we have the correct round token, continue*/
				if (local_round < tkn_buf.round && tkn_buf.is_connected) {
                    
					/* If you are process 2, reset the timing for timeouts */
					if (machine_index == 2) {
                        if (clock_gettime(USED_CLOCK, &begin)) {
                            /* Getting clock time failed */
                            exit(EXIT_FAILURE);
                        }
                        startt = begin.tv_sec*NANOS + begin.tv_nsec;
                    }

					/* Set the token, retransmit, add to rtr array, fill 
					 * frame and send*/
					tkn = tkn_buf;
					retransmit();
                    write_packets();
					fill_window();
					fill_retrans();

					/* Token aru logic */
					if (local_aru < tkn.aru || machine_index == tkn.last_lowered) {
						tkn.aru = local_aru;
						tkn.last_lowered = machine_index;
					}

					/* Termination logic */
					if((global_aru == tkn.sequence) && (tkn.sequence == last_seq)) {
                        machine_finished->type = 3;
                        machine_finished->machine_index = machine_index;
                        sendto(ss, machine_finished, sizeof(packet), 0,
                            (struct sockaddr *)&send_addr, sizeof(send_addr));
                        if (machines_finished[machine_index-1] == 0) {
                            machines_finished[machine_index-1] = 1;
                            num_finished++;
                        }
					}

					/** Calculate values **/
					global_aru = minimum(tkn.aru, last_seen_aru);
					last_seen_aru = tkn.aru;
					local_round++;
					last_seq = tkn.sequence;
				}
				
				/* Pass along the token */
                if (!(!tkn_buf.is_connected && machine_index == 1)) {
				    sendto(ss, &tkn, sizeof(token), 0, 
					    (struct sockaddr *)&neighbor, sizeof(neighbor));
                }

			/** If we receive a data packet **/		
			} else if (packet_type == 0) {
			
				/** If the packet's index > local ARU and not so high that 
				 * it will overwrite a neccessary packet, add it to recv_buf 
				 * array **/
                if (buffer->machine_index > 0) {
                    if (buffer->packet_index > local_aru) {
                        *recv_buf[buffer->packet_index % BUFFER_SIZE] = *buffer;
                    }
                    if (local_aru == -1 && recv_buf[0]->packet_index != -1) {
                        local_aru++;
                    }
                    while(recv_buf[(local_aru+1)%BUFFER_SIZE]->packet_index > -1) {
                        local_aru++;
                    }
                }
			/** If we receive a termination packet **/
			} else if (packet_type == 3) {
                if (machines_finished[buffer->machine_index] == 0) {
                    machines_finished[buffer->machine_index] = 1;
                    num_finished++;
                }
                if (num_finished == num_machines) {
                    all_machines_finished->type = 4;
                    sendto(ss, all_machines_finished, sizeof(packet), 0,
                        (struct sockaddr *)&send_addr, sizeof(send_addr));
                    break;
                }
			} else if (packet_type == 4) {
                all_machines_finished->type = 4;
                sendto(ss, all_machines_finished, sizeof(packet), 0,
                    (struct sockaddr *)&send_addr, sizeof(send_addr));
                break;
            }
		}
		/*Check if machine 2 has timed out, which means the token was lost*/
		if(machine_index == 2) {
            if (clock_gettime(USED_CLOCK, &current)) {
				/* Getting clock time failed */
                exit(EXIT_FAILURE);
            }
            elapsed = current.tv_sec*NANOS + current.tv_nsec - startt;
            microseconds = elapsed / 1000 + (elapsed % 1000 >= 500);
			if(microseconds > 100000) {
                if (clock_gettime(USED_CLOCK, &begin)) {
                    /* Getting clock time failed */
                    exit(EXIT_FAILURE);
                }
                startt = begin.tv_sec*NANOS + begin.tv_nsec;
				sendto(ss, &tkn, sizeof(token), 0, 
					(struct sockaddr *)&neighbor, sizeof(neighbor));
			}
		}
    }

	/** Free all malloc'd memory **/
	free(received_packet);
	free(i_packet);
	free(buffer);
	free(machine_finished);
	free(all_machines_finished);
	for(i = 0; i < BUFFER_SIZE; i++) {
		free(recv_buf[i]);
	}

	/* Close the file, print the final report, and terminate */
	fclose(file);
	final_report();
    return 0;

}

void final_report() {
	/* Prints out the total time taken for the transfer process */
	gettimeofday(&end, NULL);
	printf("\n*******PROCESS HAS ENDED**********");
	printf("\nTotal Time Taken in microseconds: %lu \n", 
		((end.tv_sec*1000000 + end.tv_usec)
		- (start.tv_sec * 1000000 + start.tv_usec)));
}

void write_packets() {
    int clear = -1;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        if((recv_buf[beginning]->packet_index > global_aru) || (recv_buf[beginning]->packet_index == -1)) {
            break;
        }
        fprintf(file, "%2d, %8d , %8d\n", recv_buf[beginning]->machine_index, recv_buf[beginning]->packet_index, 
            recv_buf[beginning]->random_number);
        recv_buf[beginning]->packet_index = clear;
        beginning = (beginning + 1) % BUFFER_SIZE;
    }
}

int minimum(int a, int b) {
	/* Return the minimum of two values */
	if(a < b) {
		return a;
	}else{
		return b;
	}
}

void retransmit() {
    packet temp_packet;
    for (int i = 0; i < RETRANS_SIZE; i++) {
        if (tkn.retrans[i] > 0) {
            temp_packet = *recv_buf[tkn.retrans[i] % BUFFER_SIZE];
            if (tkn.retrans[i] == temp_packet.packet_index) {
                sendto(ss, &temp_packet, sizeof(packet), 0,
                    (struct sockaddr *)&send_addr, sizeof(send_addr));
                tkn.retrans[i] = -1;
            }
        }
    }
}

void fill_retrans() {

    char not_been_set = 1;
    int temp_local_aru = local_aru+1;
    int index;

    /*iterate to end of window*/
    while(temp_local_aru <= tkn.sequence) {
        index = recv_buf[temp_local_aru % BUFFER_SIZE]->packet_index;
        for (int j = 0; j < RETRANS_SIZE; j++) {
            if (index < local_aru && tkn.retrans[j] == -1 && not_been_set) {
                tkn.retrans[j] = temp_local_aru;
                not_been_set = 0;
            } else if (tkn.retrans[j] == temp_local_aru) {
                tkn.retrans[j] = -1;
            }
        }
        temp_local_aru++;
    }
}

void fill_window() {
	int temp = ((tkn.sequence+1) % BUFFER_SIZE);

    for (int i = 0; i < BURST_SIZE; i++) {

        if (tkn.sequence+1 >= ((recv_buf[beginning]->packet_index)+WINDOW_SIZE)) {
            return;
        }

        if(sent_packets == num_packets) {
            return;
        }

        if (recv_buf[temp]->packet_index > -1) {
            return;
        }
        recv_buf[temp]->random_number = rand();
        recv_buf[temp]->type = 0;
        recv_buf[temp]->machine_index = machine_index;
        if(tkn.aru == tkn.sequence) {
            tkn.aru++;
        }
        if (tkn.sequence == local_aru) {
            local_aru++;
        }
        tkn.sequence++;
        recv_buf[temp]->packet_index = tkn.sequence;
        sendto(ss, recv_buf[temp], sizeof(packet), 0,
            (struct sockaddr *)&send_addr, sizeof(send_addr));

        sent_packets++;
        temp = ((tkn.sequence+1) % BUFFER_SIZE);

    }
}
