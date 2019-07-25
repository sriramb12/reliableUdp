/*
 *  file: server.h 
 *  Description:  header for producer
 *
 */

#include <stdlib.h>

// for later
#include <pthread.h>
// macros
#define CLI_LIVE_TIME 10
#define POLL_TIMEOUT 3000
#define MAX_SESSIONS 3

//session data macros
#define SESS_INV 0
#define SESS_FILE_ERR 0
#define SESS_DATA_OK 0
#define SESS_DATA_EOF 0

/* DATA Structures/typedefs */

#define MAX_SRV_RETRIES 3
#ifndef MAX_RES_LEN
#define MAX_RES_LEN 16
#endif
typedef struct {
  struct sockaddr_in cliAddr;
  int seq;
  int clifd;
  int resFd;
  char resName[MAX_RES_LEN];
  unsigned long size;
  unsigned short blockCount;
  int lastUpdateTime;
  int acksPending;
  struct block{
    char retries:3;
    char timeSince:5;
  } *blockInfo;
  int lastBlockSize;
  int lastBlock;
} tSrvSession;

typedef enum {
 ST_SRV_LISTEN,
 ST_SRV_INIT_ACK,
 ST_SRV_DATA_ACK
} tSrvState;


//Globals
extern tSrvSession srvSession[];

// Function protos
void server(int fd);
void cleanupStaleSessions(void);
int checkResource(char* reqFile);
int sendClientErr(struct sockaddr_in cliAddr, tSrvMsgType type);
int sendClientData();
void dumpFreeSessions();
int fillBuffers(tSrvSession* pSes);
int transfer(int sIdx);
int getFreeBlock(tSrvSession *pSes);
int sendBlock(tSrvSession *pSes, int blockId);
int sendClientMsg(tSrvMsgType type);
void reset();
void dumpStats();
void dumpSrvHeader();
void recvClientMsg(int sockfd);
void server(int fd);
int sendData();
int sendClientMsg(tSrvMsgType type);
int processDataAck();
int processConnect();
int processInit();
int processClientMsg();

//debug functions
void dumpSummary();
void dumpErrStats();
void setRandomErrorRate(int);
