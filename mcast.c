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
#define USED_CLOCK CLOCK_MONOTONIC /* CLOCK_MONOTONIC_RAW if available*/
#define NANOS 1000000000LL

void recv_dbg_init(int percent, int machine_index);
void final_report();

void send_token();

struct sockaddr_in name;
struct sockaddr_in send_addr;
struct sockaddr_in my_addr; /*Address for this machine*/
struct sockaddr_in neighbor; /*Address for the machine after*/
struct timeval     start, end;

struct timespec    begin, current;
long               startt, elapsed, microseconds;

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
token              *tkn_ack;
token              tkn;
ip_packet          *i_packet;
packet             *buffer;

int                local_aru = 0;
int                last_written = 0;
int                sent_packets = 0;
int                last_seen_aru = 0;
packet             *holding[HOLDING_SIZE];
packet             *frame[FRAME_SIZE];

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

    /*Initialize loss rate*/
    recv_dbg_init(loss_rate, machine_index);

    /*Malloc all needed memory*/
	received_packet = malloc(sizeof(packet));
	i_packet = malloc(sizeof(ip_packet));
    tkn_ack = malloc(sizeof(token));
    buffer = malloc(PACKET_SIZE);

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
            tkn.retrans_req[i] = -1; /*??*/
		}
		/*for(i = 0; i < tkn->is_finished; i++) {
            tkn->is_finished[i] = 0;
		}*/
        tkn.type = 1;
		tkn.sequence = 0;
		tkn.aru = 0;
        has_token = 1;
        if (clock_gettime(USED_CLOCK, &begin)) {
            /* Oops, getting clock time failed */
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
            printf("\n%lu\n",microseconds);
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

    printf("\nI BROKE OUT\n");
	if(has_token) {
		printf("\nI have the token\n");
		exit(0);
	}


/**************************
 * ************************
 * SINGLE RING TOKEN BEGINS HERE
 * ************************    
 * ***********************/

    for(;;)
    {
        if (has_token) {
   
			/*Logic for updating the token*/
			if((tkn.aru > local_aru) || (machine_index == tkn.last_lowered)) {
				tkn.aru = local_aru;
				tkn.last_lowered = machine_index;
			}

			/*If you have any packets in the retrans in your packet holding array, send them*/
			/*Can be more efficient ---> YES, mod the index of the missing packet, then it will be O(n)*/
			for(i = 0; i < RETRANS_SIZE; i++) {
				for(j = 0; j < HOLDING_SIZE; j++) {
					if(holding[j]->packet_index == tkn.retrans_req[i]) {
						sendto(ss,holding[j],sizeof(packet),0,(struct sockaddr *)&send_addr,sizeof(send_addr));
						break;
					}
				}
			}

			/*Add missing packets to the rtr array in the token*/
			for(i = 0; i < FRAME_SIZE; i++) {
				if((frame[i]->packet_index<last_written)&&(frame[i]->packet_index+FRAME_SIZE<=num_packets)) {
					for(j = 0; j < RETRANS_SIZE; j++) {
						if(tkn.retrans_req[i] == -1) {
							tkn.retrans_req[i] = frame[i]->packet_index+FRAME_SIZE;
							break;
						}else if(tkn.retrans_req[i] == frame[i]->packet_index+FRAME_SIZE) {
							break;
						}
					}
				}
			}

			/**/
			if(sent_packets < num_packets) {

				/*Update your frame so that it is filled and has no packets
				 * which have been received by all processes*/
				/*sent_packets++*/
			}else{
				if((local_aru == tkn.sequence) && (tkn.sequence==tkn.aru)) {
					/*Finishing logic*/
				}
			}

			/*Multicast all packets in your frame*/
			for(i = 0; i < FRAME_SIZE; i++) {
				
				/*Multicast the packet*/
				sendto(ss, frame[i], sizeof(packet),0,(struct sockaddr *)&send_addr, sizeof(send_addr));

				/*Update token -- is the first if right???*/
				if((tkn.aru == tkn.sequence) && (local_aru == tkn.aru)) {
					tkn.aru++;
				}
				if(frame[i]->packet_index > tkn.sequence) {
					tkn.sequence = frame[i]->packet_index;
				}

			}

			/*Set the last seen ARU*/
			last_seen_aru = tkn.aru;

			/*Send the token to the neighbor processes*/
			sendto(ss, &tkn, sizeof(token), 0, (struct sockaddr *)&neighbor, sizeof(neighbor));


		}else{
			/*If receive a token and already have a token, send an ack*/
			temp_mask = mask;
			num = select(FD_SETSIZE,&temp_mask,&dummy_mask,&dummy_mask,
				&timeout);
            if (num > 0) {
                bytes = recv_dbg( sr, (char *) buffer, PACKET_SIZE, 0 );
                packet_type = buffer->type;
                if (packet_type == 1) {
					tkn = *((token *)buffer);
					if(!tkn.is_connected) {
						printf("\nI sent the token! From main loop\n");
						sendto( ss, &tkn, sizeof(token), 0, 
						(struct sockaddr *)&neighbor, sizeof(neighbor) );
					}
                }
            }
		}
    }

	final_report();
    return 0;

}

void final_report() {
	gettimeofday(&end, NULL);
	printf("\n*******PROCESS HAS ENDED**********");
	printf("\nTotal Time Taken: %lu \n", ((end.tv_sec*1000000 + end.tv_usec)
		- (start.tv_sec * 1000000 + start.tv_usec)));
}
