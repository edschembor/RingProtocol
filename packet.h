#define SIZE 1212
#define PAYLOAD 1200

typedef struct packet_struct{

	char data[PAYLOAD];
	int process_index;
	int packet_index;
	int random_number;

} packet;
