#include "global.h"

int stratumClient_openConnection(char *IP, int Port)
{
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(Port);
	addr.sin_addr.s_addr = inet_addr(IP);
	int result = connect(s, (sockaddr *) & addr, sizeof(sockaddr_in));

	if (result < 0) {
		return 0;
	}
	return s;
}

void stratumClient_free(stratumClient_t* stratumClient){
	while(!stratumClient->shares.empty()){
		char* buf = stratumClient->shares.front();
		delete buf;
		stratumClient->shares.pop_front();
	}
	delete stratumClient;
}

void stratumClient_subscribe(stratumClient_t* client){
	const char *subStr = "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": []}\n";

	send(client->clientSocket, subStr, strlen(subStr), 0);
}

void stratumClient_login(stratumClient_t* client){
	char buf[1024];
	sprintf(buf,"{\"params\": [\"%s\", \"%s\"], \"id\": 2, \"method\": \"mining.authorize\"}\n",
			client->username, client->password); 

	send(client->clientSocket, buf, strlen(buf), 0);
}

void stratumClient_foundShare(stratumClient_t* client, primecoinBlock_t* pblock){
	//{"params": ["slush.miner1", "bf", "00000001", "504e86ed", "b2957c02"], "id": 4, "method": "mining.submit"}
	//Values in particular order: worker_name (previously authorized!), job_id, extranonce2, ntime, nonce.
	char mult[128];
	mpz_get_str(mult,16,pblock->mpzPrimeChainMultiplier.get_mpz_t());

#if 0
	printf("%s\n", mult);

	uint32_t i;
	for(i=0; i < 80; i++){
		printf("%2.2X ", ((unsigned char*)pblock)[i]);
	}
	printf("\n");
#endif
/*	
	uint32_t mlen=strlen(mult);
	
	for(i=0; i < mlen; i++){
		mult[95-i] = mult[mlen-i-1];
	}	
	for(i=0; i < 96-mlen; i++){
		mult[i] = '0';
	}	
	mult[96] = 0;

	printf("%s\n", mult);
*/
	char *buf = new char[1024];
	pthread_mutex_lock(&client->cs_shareSubmit);
	sprintf(buf,"{\"params\": [\"%s\",\"%8.8x\",\"%8.8x\",\"%8.8x\",\"%8.8x\",\"%s\"],\"id\": %d, \"method\": \"mining.submit\"}\n",
			client->username, pblock->serverData.blockHeight, htonl(pblock->extraNonce), htonl(pblock->timestamp), htonl(pblock->nonce), mult, client->nextId++); 

	client->shares.push_back(buf);
	pthread_mutex_unlock(&client->cs_shareSubmit);
}

stratumClient_t *stratumClient_connect(jsonRequestTarget_t * target)
{
	int clientSocket =
	    stratumClient_openConnection(target->ip, target->port);
	if (clientSocket == 0)
		return NULL;

       	// set socket as non-blocking
	int flags, err;
	flags = fcntl(clientSocket, F_GETFL, 0);
	flags |= O_NONBLOCK;
	err = fcntl(clientSocket, F_SETFL, flags);	//ignore errors for now..

	// initialize the client object
	stratumClient_t *stratumClient = new stratumClient_t();
	stratumClient->clientState=0;
	stratumClient->writeIndex=0;
	stratumClient->readIndex=0;
	stratumClient->jsonIndex=0;
	stratumClient->disconnected=0;
	stratumClient->disconnectReason=0;
	stratumClient->clientSocket = clientSocket;
	stratumClient->nextId = 3;
//	stratumClient->sendBuffer = xptPacketbuffer_create(64 * 1024);
	fStrCpy(stratumClient->username, target->authUser, 127);
	fStrCpy(stratumClient->password, target->authPass, 127);
	pthread_mutex_init(&stratumClient->cs_shareSubmit, NULL);
//	xptClient->list_shareSubmitQueue = simpleList_create(4);
	// send worker login
	stratumClient_subscribe(stratumClient);
//	xptClient_sendWorkerLogin(xptClient);
	// return client object

	return stratumClient;
}

bool stratumClient_isDisconnected(stratumClient_t* stratumClient, char** reason){
	if (reason)
		*reason = stratumClient->disconnectReason;
	return stratumClient->disconnected;
}

bool stratumClient_isAuthenticated(stratumClient_t* stratumClient){
	return (stratumClient->clientState == STRATUM_CLIENT_STATE_LOGGED_IN);
}

bool stratumClient_handleErr(stratumClient_t *client, jsonObject_t* obj){
	//TODO: decode and print the errors
	jsonObject_t* jsonErr = jsonObject_getParameter(obj, "error");
	if(jsonErr){
		if(jsonObject_getType(jsonErr) != JSON_TYPE_NULL){
			int errid = jsonObject_getNumberValueAsS32(jsonErr);
			switch(errid){
				case 21: printf("Share rejected - Share stale\n"); break;
				case 22: printf("Share rejected - Duplicate share\n"); break;
				case 23: printf("Share rejected - Difficulty too low\n"); break;
				default: printf("Protocol Error %d\n", errid); return false;
			}
			return false;
		}
	}else{
		return true;
	}
	return true;
}


bool stratumClient_validateLogin(stratumClient_t *client, jsonObject_t* obj){
	//Validate id of login response
//	printf("Got auth\n");
	jsonObject_t* jsonId = jsonObject_getParameter(obj, "id");
	if(jsonId){
		int id = jsonObject_getNumberValueAsS32(jsonId);
		if(id!=2)
			return false;
	}else{
		return false;
	}


	jsonObject_t *jsonResult = jsonObject_getParameter(obj, "result");
	if(!jsonResult)
		return false;

	if(jsonObject_isTrue(jsonResult)){
		client->clientState = STRATUM_CLIENT_STATE_LOGGED_IN;
		return true;
	}
	return false;
}

//Convert to little endian hex
void leString(char *s, int len){
	char buf[256];
	int i=0;
	len--;
	while(len>=0){
		char c1 = s[len--];

		char c2 = '0';
		if(len>=0)
			c2 = s[len--];

		buf[i++] = c2;
		buf[i++] = c1;
	}

	//Copy over the shit
	int j;
	for(j=0; j < i; j++){
		s[j] = buf[j];
	}
	s[j] = 0;
}

int xtoi(char c)
{
    int v = -1;
    char w=toupper(c);
    if(w >= 'A' && w <= 'F'){
        v = w - 'A' + 0x0A;
    }else if (w >= '0' && w <= '9'){
        v = w - '0';
    }

    return v;
}

uint8_t readbyte(char *d){
	char c2 = d[0];
	char c1 = d[1];

	return xtoi(c2) * 16 + xtoi(c1);
}

bool stratumClient_toBytes(stratumClient_t *client, jsonObject_t* obj, void* vdest, int nBytes, uint32_t *lren=0){
	uint8_t *dest = (uint8_t*)vdest;
	uint32_t length=0;
	uint8_t* string = jsonObject_getStringData(obj,&length);
	if(length==0 || length >= 256)
		return false;

	//printf("%d\n", length);
	
	char buf[256];
	strncpy(buf,(const char *)string,length);
	buf[length] = 0;

	//leString(buf,length);

	//Convert to bytes
	length = strlen(buf);
	int i;
	for(i=0; i < nBytes; i++){
		if(i*2 < length){
			dest[i] = readbyte(buf+i*2);
		}else{
			dest[i] = 0;
		}
	}
	if(lren)
		*lren = length/2;

	return true;
}

bool stratumClient_toBytesLE(stratumClient_t *client, jsonObject_t* obj, void* vdest, int nBytes){
	uint8_t *dest = (uint8_t*)vdest;
	uint32_t length=0;
	uint8_t* string = jsonObject_getStringData(obj,&length);
	if(length==0 || length >= 256)
		return false;

	//printf("%d\n", length);
	
	char buf[256];
	strncpy(buf,(const char *)string,length);
	buf[length] = 0;

	leString(buf,length);

	//Convert to bytes
	length = strlen(buf);
	int i;
	for(i=0; i < nBytes; i++){
		if(i*2 < length){
			dest[i] = readbyte(buf+i*2);
		}else{
			dest[i] = 0;
		}
	}

	return true;
}

bool stratumClient_validateSubscription(stratumClient_t *client, jsonObject_t* obj){

	//Validate id of subscription response
	jsonObject_t* jsonId = jsonObject_getParameter(obj, "id");
	if(jsonId){
		int id = jsonObject_getNumberValueAsS32(jsonId);
		if(id!=1)
			return false;
	}else{
		return false;
	}

	//Should hopefully be valid now

	jsonObject_t *jsonResult = jsonObject_getParameter(obj, "result");
	if(!jsonResult)
		return false;

	if(jsonObject_getArraySize(jsonResult) != 3)
		return false;


	jsonObject_t *jsonDetails = jsonObject_getArrayElement(jsonResult, 0);
	if(jsonObject_getArraySize(jsonDetails) != 2)
		return false;


	jsonObject_t *jsonN1 = jsonObject_getArrayElement(jsonResult, 1);
	jsonObject_t *jsonN2Size = jsonObject_getArrayElement(jsonResult, 2);

	int n2size = jsonObject_getNumberValueAsS32(jsonN2Size);

	if(!stratumClient_toBytes(client, jsonN1, &client->nonce1, 4))
		return false;

	client->Nonce2Bytes = n2size;

	client->clientState = STRATUM_CLIENT_STATE_SUBSCRIBED;
	
	return true;
}

bool stratumClient_processDifficulty(stratumClient_t *client, jsonObject_t* obj){
//	printf("Recieved diff\n");


	jsonObject_t *jsonParams = jsonObject_getParameter(obj, "params");
	if(!jsonParams){
		printf("No params\n");
		return false;
	}

	if(jsonObject_getArraySize(jsonParams) != 1){
		printf("Array size wrong\n");
		return false;
	}

	if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonParams,0),&client->difficulty,4)){
		printf("Failed to read version\n");
		return false;
	}

	if(client->clientState==STRATUM_CLIENT_STATE_SUBSCRIBED)
		client->clientState = STRATUM_CLIENT_STATE_GOT_DIFF;
	return true;
}

bool stratumClient_processNotify(stratumClient_t *client, jsonObject_t* obj){
	int i;
//	printf("Received notify\n");

	jsonObject_t *jsonParams = jsonObject_getParameter(obj, "params");
	if(!jsonParams){
		printf("No params\n");
		return false;
	}

	if(jsonObject_getArraySize(jsonParams) != 9){
		printf("Array size wrong\n");
		return false;
	}

	//TODO: lock mutex!

	if(!stratumClient_toBytesLE(client,jsonObject_getArrayElement(jsonParams,0),&client->blockHeight,4)){
		printf("Bad blockheight\n");
		return false;
	}

	if(!stratumClient_toBytesLE(client,jsonObject_getArrayElement(jsonParams,1),&client->prevHash,32)){
		printf("Bad prevhash\n");
		return false;
	}
	
	if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonParams,2),&client->coinB1,COINB1_SIZE,&client->coinb1_size)){
		printf("Bad coinb1\n");
		return false;
	}
	
	if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonParams,3),&client->coinB2,COINB2_SIZE,&client->coinb2_size)){
		printf("Bad coinb2\n");
		return false;
	}

	//Merkle branch
	jsonObject_t *jsonArray = jsonObject_getArrayElement(jsonParams,4);
	if(jsonObject_getType(jsonArray) != JSON_TYPE_ARRAY){
		printf("Bad merkle branch\n");
		return false;
	}

	client->nMerkle = jsonObject_getArraySize(jsonArray);
	if(client->nMerkle+1 > MAX_MERKLES){
		printf("Too much merkle\n");
		return false;
	}

	for(i=0; i < client->nMerkle; i++){
		if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonArray,i),&client->merkles[(i+1)*32],32)){
			printf("Failed to read merkle\n");
			return false;
		}
	}

	if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonParams,5),&client->version,4)){
		printf("Failed to read version\n");
		return false;
	}

	if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonParams,6),&client->nbits,4)){
		printf("Nbits failed\n");
		return false;
	}

//	printf("Network difficulty: %f\n", GetPrimeDifficulty(client->nbits));
	
	if(!stratumClient_toBytes(client,jsonObject_getArrayElement(jsonParams,7),&client->ntime,4)){
		printf("Ntime failed\n");
		return false;
	}

	//Don't care about element(8) clean jobs. 
//	printf("Job success\n");

	return true;
}

bool stratumClient_processMethod(stratumClient_t *client, jsonObject_t* obj, jsonObject_t* jsonMethod){
	char method[64];
	uint32_t methodLength=0;
	uint8* methodS = jsonObject_getStringData(jsonMethod, &methodLength);
	if(methodS==NULL || methodLength == 0)
		return false;

	strncpy(method,(const char *)methodS,methodLength);
	method[methodLength] = 0;

	
	if(strncmp(method,"mining.notify", methodLength)==0)
		return stratumClient_processNotify(client,obj);

	if(strncmp(method,"mining.set_difficulty", methodLength)==0)
		return stratumClient_processDifficulty(client,obj);

	//Unknown method call
	std::cout << "Unknown method call: " << method << "\n" << std::endl;
	return false;
}

bool stratumClient_processResult(stratumClient_t *client, jsonObject_t* obj, jsonObject_t* jsonResult){
	int result = jsonObject_getNumberValueAsS32(jsonResult);
	printf("Remote returned: %d\n", result);

	return true;
}


bool stratumClient_parsePacket(stratumClient_t* client, char* buf, int length){
#if 0
	printf("RXd: ");
	int i;
	for(i=0; i < length; i++)
		printf("%c", buf[i]);
	printf("\n");
#endif
	jsonObject_t* obj = jsonParser_parse((uint8*)buf,length);
	if(!obj){
		std::cout << "Received invalid object!" << std::endl;
		return false;
	}

	if(!stratumClient_handleErr(client,obj))
		return false;

	if(client->clientState == STRATUM_CLIENT_STATE_NEW){
		if(stratumClient_validateSubscription(client,obj)){
			stratumClient_login(client);
			return true;
		}
		return false;
	}

	//Terrible ugly hack
	if(client->clientState == STRATUM_CLIENT_STATE_GOT_DIFF){
		jsonObject_t* jsonId = jsonObject_getParameter(obj, "id");
		if(jsonId){
			int id = jsonObject_getNumberValueAsS32(jsonId);
			if(id==2)
			return stratumClient_validateLogin(client,obj);
		}
	}

	//Check	if method call
	jsonObject_t* jsonMethod = jsonObject_getParameter(obj, "method");
	if(jsonMethod){
//		printf("Method call\n");
		return stratumClient_processMethod(client,obj,jsonMethod);
	}

	jsonObject_t* jsonResult = jsonObject_getParameter(obj, "result");
	if(jsonResult){
		return stratumClient_processResult(client,obj,jsonResult);
	}

	//Success for no particular reason
	jsonObject_freeObject(obj);
	return true;
}

bool stratumClient_process(stratumClient_t* stratumClient){
	if (stratumClient == NULL || stratumClient->disconnected){
		std::cout << "Disconnected!" << std::endl;
		return false;
	}

	//printf("process\n");

	//send shares
	pthread_mutex_lock(&stratumClient->cs_shareSubmit);
	while(!stratumClient->shares.empty()){
		char* buf = stratumClient->shares.front();
		send(stratumClient->clientSocket, buf, strlen(buf), 0);
		delete buf;
		stratumClient->shares.pop_front();
	}
	pthread_mutex_unlock(&stratumClient->cs_shareSubmit);
	
	// check for packets
	uint32_t modindex = stratumClient->writeIndex % STRATUM_BUF_SIZE;
	sint32 r = recv(stratumClient->clientSocket,
		 (char *)(stratumClient->recvBuffer + modindex), STRATUM_BUF_SIZE - modindex, 0);
	if (r <= 0) {
		// receive error, is it a real error or just because of non blocking sockets?
		if (r==0 || errno != EAGAIN) {
			std::cout << "Connection error!" << std::endl;
			stratumClient->disconnected = true;
			return false;
		}
		return true;
	}
	stratumClient->writeIndex +=r;
	if(stratumClient->writeIndex - stratumClient->readIndex > STRATUM_BUF_SIZE){
		std::cout << "Recv buffer overflow!" << std::endl;
		stratumClient->disconnected = true;
		return false;
	}

	//process the data
	while(stratumClient->readIndex < stratumClient->writeIndex){
		char c = stratumClient->recvBuffer[stratumClient->readIndex++ % STRATUM_BUF_SIZE];
		if(c=='\n'){
			//Do it;
			if(!stratumClient_parsePacket(stratumClient,stratumClient->jsonBuf,stratumClient->jsonIndex)){
				std::cout << "Bad stratum message!" << std::endl;
				stratumClient->disconnected = true;
				return false;
			}
			stratumClient->jsonIndex=0;
		}else{
			if(stratumClient->jsonIndex >= STRATUM_BUF_SIZE){
				std::cout << "Json message length overflow!" << std::endl;
				stratumClient->disconnected=true;
				return false;
			}
			stratumClient->jsonBuf[stratumClient->jsonIndex++] = c;
		}
	}	
	return true;
}
