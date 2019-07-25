/*
 *  file: consumer.h 
 *  Functionality: implements a standalone network stream consumer
 *     using reliable udp 
 *
 */


// macros
#define SERVER_CONN_TIMEOUT 2000
#define MAX_CLIENT_RETRIES 3
#define MAX_DISK_WRITE_FAILS 3

//Define how many blocks we can buffer  in case of out of seq delivery?
#define MAX_BLOCK_RANGE 0
/* DATA Structures/typedefs */

//client states
typedef enum {
 ST_CLI_INIT_ACK,
 ST_CLI_CONNECT_ACK,
 ST_CLI_DATA
} tCliState;
tCliState myState = ST_CLI_INIT_ACK;

typedef struct resource {
    char resName[MAX_RES_LEN];
    unsigned avblDiskSpace;
    unsigned size;
    int seq; //starts from 1 (TBD: can start with a random number seq, for robustness)
    int srvAddr, srvfd, clifd;
    int writeFd;
    int cliPort;
    int numPktsRx;
    int blockCount;
    int rxBlocks, rxBytes;
    int blocks[0xffff];
    int dataBufIndx[MAX_BLOCK_RANGE];
    char filename[200];
    tSrvMsgData srvMsgBuf;
}tClientContext;

extern char* srvMsgTypeToStr[];
struct stats {
   int invalidPkts;
   int outofRangePkts;
   int duplicatePkts;
   int diskWriteFails;
   int sendFails;
   char duplicateBlocks;
} relUdpStats;



//Globals
#define STORE_PATH "/tmp/reliableUdpClient"

// Function protos
int runClient();
int talkToSvr();
tCliMsg buildCliMsg(tCliMsgType type, char* fname, int ack);
int processData();
void processAck(int sIdx, tCliMsg cliMsg);
int transfer(int sIdx);
int writeToDisk();
int handleServMsg();
int sendMsgToSvr(tCliMsgType type);
int createDataFile();
int processInitAck();
int writeDataToDisk();
int processData();
int handleServMsg();
int process();
int runClient();
int validateDataMsg(tSrvMsgHdr *pSrvMsg);
int main(int argc, char **av);
int sendMsgToSvr(tCliMsgType type);
void init(char** av);
void dumpDataPkt(tSrvMsgHdr *header);
