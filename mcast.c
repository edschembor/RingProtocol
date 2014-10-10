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

void recv_dbg_init(int percent, int machine_index);

void send_token();

struct sockaddr_in name;
struct sockaddr_in send_addr;
struct sockaddr_in my_addr; /*Address for this machine*/
struct sockaddr_in neighbor; /*Address for the machine after*/

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

int                ss,sr,i;
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

int main(int argc, char **argv)
{

    /*Make sure the usage is correct*/
	if(argc != 5) {
        printf("Usage: mcast <num_packets> <machine_index> <num_of_machines> <loss_rate>\n");
		exit(0);
	}

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
    /* the interface could be changed to a specific interface if needed */
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
            tkn.retransmission_request[i] = -1; /*??*/
		}
		/*for(i = 0; i < tkn->is_finished; i++) {
            tkn->is_finished[i] = 0;
		}*/
        tkn.type = 1;
		tkn.sequence = 0;
		tkn.aru = 0;
        tkn.from_addr = my_addr;
        has_token = 1;
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

        /*If I've seen the token and I've sent it successfully, I end*/
        /*if (seen_token && sent_token) {*/
        if (seen_token) {
            break;
        }

        /*If I haven't seen the token, multicast*/
        if(seen_token == 0) {
            sendto( ss, i_packet, sizeof(ip_packet), 0, 
                (struct sockaddr *)&send_addr, sizeof(send_addr) );
        }

        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);

        if (num > 0) {
            /** receive packet **/
            bytes = recv_dbg( sr, (char *) buffer, PACKET_SIZE, 0 );
            packet_type = buffer->type;

            /** If token **/
            if (packet_type == 1) {
                printf("\nI got a token and it's %d big!\n", bytes);

                /** Check that it's not a rogue ack **/
                if (((token *)buffer)->aru != -1) {
                    /** Set booleans, you've seen and have the token **/
                    has_token = 1;
                    seen_token = 1;

                    /** set the token ack and send it **/
                    tkn_ack->type = 1;
                    tkn_ack->aru = -1;
                    tkn_ack->from_addr = my_addr; /** for testing, not necessary **/

                    printf("received token from: %s\this aru is: %d\n", inet_ntoa(((token *)buffer)->from_addr.sin_addr), ((token*)buffer)->aru);
     
                    sendto( ss, tkn_ack, sizeof(token), 0, 
                        (struct sockaddr *)&(((token *)buffer)->from_addr), sizeof(((token *)buffer)->from_addr) );
                    printf("\nI sent an ack, here's the proof: %d %d", tkn_ack->aru, tkn_ack->type);

                    /** shitty hack. delete **/
                    if (machine_index == 1) {
                        break;
                    }

                    /** set token for sending to neighbor **/
                    tkn = *((token *) buffer);
                    tkn.from_addr = my_addr;
               }
            /** If ip_packet **/
            } else if (packet_type == 2) {
                /** Check that the ip_packet's index is your neighbor**/
                if (((ip_packet *) buffer)->machine_index == ((machine_index % num_machines) + 1)) {
                    /** Set the ip_packet's address to your neighbor**/
                    neighbor = ((ip_packet *) buffer) -> addr;
                    has_neighbor = 1;
                    printf("\nI found my neighbor! His address is: %s\n", inet_ntoa(((ip_packet*) buffer)-> addr.sin_addr));
                }
            }
        }

        /** If you have a token and your neighbor, send the token **/
        if (has_token && has_neighbor) {
            printf("\nI'm about to send a token!\n");
            send_token();
        }
    }

    printf("\nI BROKE OUT\n");


/**************************
 * ************************
 * SINGLE RING TOKEN BEGINS HERE
 * ************************    
 * ***********************/

    for(;;)
    {
        if (has_token) {
            temp_mask = mask;
            num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
            if (num > 0) {
                bytes = recv_dbg( sr, (char *) buffer, PACKET_SIZE, 0 );
                packet_type = buffer->type;
                if (packet_type == 1) {
                    if (((token *)buffer)->aru != -1) {
                        tkn_ack->aru = -1;
                        tkn_ack->type = 1;
                        tkn_ack->from_addr = my_addr;
                        sendto( ss, tkn_ack, sizeof(token), 0, 
                            (struct sockaddr *)&(((token *)buffer)->from_addr), sizeof(((token *)buffer)->from_addr) );
                    }
                }
            }
        }
    }

    return 0;

}

void send_token() {
    for (;;) {
        /** multicast in case processes don't know your address **/
        sendto( ss, i_packet, sizeof(ip_packet), 0, 
            (struct sockaddr *)&send_addr, sizeof(send_addr) );

        /** send token **/
        sendto( ss, &tkn, sizeof(token), 0, 
            (struct sockaddr *)&neighbor, sizeof(neighbor) );

        /** wait for an ack, if timeout, send another token **/
        temp_mask = mask;
        timeout.tv_usec = 50000;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0) {
            bytes = recv_dbg( sr,(char *) buffer, PACKET_SIZE, 0 );
            /** if received is an ack, break **/
            if ((buffer->type == 1)) {
                if (((token *)buffer)->aru == -1) {
                    printf("\nI got the ack! It's from %s\n", inet_ntoa(((token *)buffer)->from_addr.sin_addr));
                    has_token = 0;
                    sent_token = 1;
                    break;
                } else if (((token *)buffer)->aru == 0) {
                    /** This means token has made it around despite lack of acks, stop expecting acks, start sending acks, and break**/
                    tkn = *((token *)buffer);
                    has_token = 1;
                    seen_token = 1;
                    tkn_ack->aru = -1;
                    tkn_ack->type = 1;
                    tkn_ack->from_addr = my_addr;
                    sendto( ss, tkn_ack, sizeof(token), 0, 
                        (struct sockaddr *)&(((token *)buffer)->from_addr), sizeof(((token *)buffer)->from_addr) );
                    printf("\nI sent an ack, here's the proof: %d %d\n-------------\nMy token was from %s\n", tkn_ack->aru, tkn_ack->type, inet_ntoa(((token *)buffer)->from_addr.sin_addr));
                    break;
                }
            }
        }
    }
}
