#define NAME_LENGTH 80

typedef struct ip_packet_struct {

    int type;
    int machine_index;
	struct sockaddr_in my_addr;

} ip_packet;
