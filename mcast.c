/* John An and Edward Schembor
 * Distributed Systems
 * Exercise 2
 * mcast program
 */

#include "net_include.h"
#include "token.h"
#include "packet.h"

int main(int argc, char **argv)
{
    struct sockaddr_in name;
    struct sockaddr_in send_addr;

    int                mcast_addr;

    struct ip_mreq     mreq;
    unsigned char      ttl_val;

    int                ss,sr,i;
    fd_set             mask;
    fd_set             dummy_mask,temp_mask;
    int                bytes;
    int                num;
    char               mess_buf[MAX_MESS_LEN];
    char               input_buf[80];
	packet             *received_packet;
	int                num_packets;
	int                machine_index;
	int                num_machines;
	int                loss_rate;
	token              *tkn;

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
	tkn = malloc(sizeof(token));

	/*Creates the mcast address*/
    mcast_addr = 225 << 24 | 1 << 16 | 2 << 8 | 103; /* (225.1.2.103) */

    sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving */
    if (sr<0) {
        perror("Mcast: socket");
        exit(1);
    }

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


    /*Wait for the special start packet from start_mcast*/
	for(;;)
	{
	    num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
	        bytes = recv( sr, (char *) received_packet, SIZE, 0 );
		    if(received_packet->process_index == -1) {
			    printf("\nGOT IT\n");
                break;
		    }
	}

    /*Connect this process to the next process*/
    

	/*The first process creates the initial token*/
    if(machine_index == 1) {
        for(i = 0; i < RETRANS_SIZE; i++) {
            retransmission_request[i] = -1; /*??*/
		}
		for(i = 0; i < is_finished; i++) {
            is_finished[i] = 0;
		}
		sequence = 0;
		aru = 0;
	}


    for(;;)
    {
        temp_mask = mask;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
        if (num > 0) {
            if ( FD_ISSET( sr, &temp_mask) ) {
                bytes = recv( sr, mess_buf, sizeof(mess_buf), 0 );
                mess_buf[bytes] = 0;
                printf( "received : %s\n", mess_buf );
            }else if( FD_ISSET(0, &temp_mask) ) {
                bytes = read( 0, input_buf, sizeof(input_buf) );
                input_buf[bytes] = 0;
                printf( "there is an input: %s\n", input_buf );
                sendto( ss, input_buf, strlen(input_buf), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr) );
            }
        }
    }

    return 0;

}

