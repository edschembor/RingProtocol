#define RETRANS_SIZE 300
#define PROCESS_COUNT 10

typedef struct token_struct {
	
    int type;
	int sequence;
	int aru;
	struct sockaddr_in from_addr;
	int retransmission_request[RETRANS_SIZE];
	int is_finished[PROCESS_COUNT];

} token;
