/* John An and Edward Schembor
 * Distributed Systems
 * Exercise 2
 * start_mcast program
 */

#include "net_include.h"
#include "packet.h"

int main()
{
    struct sockaddr_in send_addr;
    int                mcast_addr;
    unsigned char      ttl_val;

    int                ss;

	packet      *start_packet;

    mcast_addr = 225 << 24 | 1 << 16 | 2 << 8 | 103; /* (225.1.2.103) */

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

	start_packet = malloc(sizeof(packet));
	start_packet->machine_index = -1;
	printf("\nIndex: %d\n", start_packet->machine_index);
    sendto( ss, (char *) start_packet, PACKET_SIZE, 0, (struct sockaddr *)&send_addr, sizeof(send_addr) );


    return 0;

}

