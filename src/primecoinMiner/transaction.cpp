#include"global.h"

void bitclient_addVarIntFromStream(stream_t* msgStream, uint64 varInt)
{
	if( varInt <= 0xFC )
	{
		stream_writeU8(msgStream, (uint8)varInt);
		return;
	}
	else if( varInt <= 0xFFFF )
	{
		stream_writeU8(msgStream, 0xFD);
		stream_writeU16(msgStream, (uint16)varInt);
		return;
	}
	else if( varInt <= 0xFFFFFFFF )
	{
		stream_writeU8(msgStream, 0xFE);
		stream_writeU32(msgStream, (uint32)varInt);
		return;
	}
	else
	{
		stream_writeU8(msgStream, 0xFF);
		stream_writeData(msgStream, &varInt, 8);
		return;
	}
}

void bitclient_generateTxHash(uint32 nonce1, uint32 userExtraNonceLength, uint8* userExtraNonce, uint32 coinBase1Length, uint8* coinBase1, uint32 coinBase2Length, uint8* coinBase2, uint8* txHash)
{
	stream_t* streamTXData = streamEx_fromDynamicMemoryRange(1024*32);
	stream_writeData(streamTXData, coinBase1, coinBase1Length);
	stream_writeData(streamTXData, &nonce1, 4);
	stream_writeData(streamTXData, userExtraNonce, userExtraNonceLength);
	stream_writeData(streamTXData, coinBase2, coinBase2Length);
	sint32 transactionDataLength = 0;
	uint8* transactionData = (uint8*)streamEx_map(streamTXData, &transactionDataLength);
	// special case, we can use the hash of the transaction
#if 0
	int i;
	printf("Merkle:\n");
	for(i=0; i < transactionDataLength; i++){
		printf("%2.2x ", transactionData[i]); 
	}
	printf("\n");
#endif

	uint8 hashOut[32];
	sha256_context sctx;
	sha256_starts(&sctx);
	sha256_update(&sctx, transactionData, transactionDataLength);
	sha256_finish(&sctx, hashOut);
	sha256_starts(&sctx);
	sha256_update(&sctx, hashOut, 32);
	sha256_finish(&sctx, txHash);
	free(transactionData);
	stream_destroy(streamTXData);
}

void bitclient_calculateMerkleRoot(uint8* txHashes, uint32 numberOfTxHashes, uint8* merkleRoot)
{
	if(numberOfTxHashes <= 0 )
	{
		printf("bitclient_calculateMerkleRoot: Block has zero transactions (not even coinbase)\n");
		memset(merkleRoot, 0, 32);
		return;
	}
	{
		// generate transaction data
		memcpy(merkleRoot, txHashes, 32);
	}
	if( numberOfTxHashes > 1 )
	{
		// build merkle root tree
		uint8 hashData[64];
		for(uint32 i=1; i<numberOfTxHashes; i++)
		{
			memcpy(hashData, merkleRoot, 32);
			memcpy(hashData+32,txHashes+i*32, 32);

#if 0
			printf("Merkle: %d\n", i);
			for(uint32 j=0; j < 64; j++){
				printf("%2.2x ", hashData[j]); 
			}
			printf("\n");
#endif
			uint8 hashOut[32];
			sha256_context sha256_ctx;
			sha256_starts(&sha256_ctx);
			sha256_update(&sha256_ctx, hashData, 32*2);
			sha256_finish(&sha256_ctx, hashOut);
			sha256_starts(&sha256_ctx);
			sha256_update(&sha256_ctx, hashOut, 32);
			sha256_finish(&sha256_ctx, merkleRoot);
		}
	}
}
