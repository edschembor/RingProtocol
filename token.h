#define RETRANS_SIZE 1000
#define PROCESS_COUNT 10

typedef struct token_struct {
	
	int retransmission_request[RETRANS_SIZE];
	int is_finished[PROCESS_COUNT];
	int sequence;
	int aru;

} token;
