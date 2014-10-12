#define RETRANS_SIZE 300
#define PROCESS_COUNT 10

typedef struct token_struct {
	
    int type;
	int sequence;
	int aru;
	int last_lowered;
	int retrans_req[RETRANS_SIZE];
	char is_connected;

} token;
