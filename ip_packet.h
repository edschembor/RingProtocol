#define NAME_LENGTH 80

typedef struct ip_packet_struct {

    int type;
    int machine_index;
	struct sockaddr_in addr;

} ip_packet;
