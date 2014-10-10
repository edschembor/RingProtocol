#define PACKET_SIZE 5000 /*change back to 1212*/
#define PAYLOAD 1200

typedef struct packet_struct{

    int type;
	int machine_index;
	int packet_index;
	int random_number;
	char data[PAYLOAD];

} packet;
