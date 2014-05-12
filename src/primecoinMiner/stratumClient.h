#ifndef STRATUM_H
#define STRATUM_H

#include <list>

#define STRATUM_CLIENT_STATE_NEW		(0)
#define STRATUM_CLIENT_STATE_SUBSCRIBED		(1)
#define STRATUM_CLIENT_STATE_GOT_DIFF   	(2)
#define STRATUM_CLIENT_STATE_LOGGED_IN		(3)

#define STRATUM_BUF_SIZE (1024*1024)


#define COINB1_SIZE 64 //not sure of real size here
#define COINB2_SIZE 64

#define MAX_MERKLES 4096

using namespace std;


typedef struct {
	int clientState;
 	int clientSocket;
	int nextId;

	//locks
	pthread_mutex_t cs_shareSubmit;

	// worker info
	char username[128];
	char password[128];

	//Recv stuff
	uint32_t writeIndex;
	uint32_t readIndex;
	char recvBuffer[STRATUM_BUF_SIZE];
	char jsonBuf[STRATUM_BUF_SIZE];
	int jsonIndex;

	char* disconnectReason;
	bool disconnected;

	//Block template stuff
	uint64_t blockHeight;
	uint8_t prevHash[32];
	uint8_t coinB1[COINB1_SIZE]; 
	uint8_t coinB2[COINB2_SIZE];
	uint32_t coinb1_size;
	uint32_t coinb2_size;
	uint32_t version;
	uint32_t nbits;
	uint32_t ntime;
	uint32_t nMerkle; 
	uint8_t merkles[MAX_MERKLES*32];

	uint32_t nonce1;
	int Nonce2Bytes;

	int difficulty;
	list<char*> shares;
} stratumClient_t;

stratumClient_t* stratumClient_connect(jsonRequestTarget_t* target);
bool stratumClient_isDisconnected(stratumClient_t* stratumClient, char** reason);
bool stratumClient_isAuthenticated(stratumClient_t* stratumClient);
bool stratumClient_process(stratumClient_t* stratumClient); // needs to be called in a loop
void stratumClient_free(stratumClient_t* stratumClient);
void stratumClient_foundShare(stratumClient_t* stratumClient, primecoinBlock_t* pblock);

#endif //STRATUM_H
