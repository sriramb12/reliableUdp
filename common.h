/*
 *  file: common.h 
 *  Description:  common header for code
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h> 
#include <unistd.h> 
#include<time.h> 
#include <sys/stat.h>
#include <fcntl.h>


// macros
#define SRV_PORT 1111

// Important: choose such that it fits within allowed UDP packet including application header
// Ex: if UDP segment is 64k, MAX_BLOCK_SIZE  could be 64k - sizeof server header 
// Tested MAX_BLOCK_SIZE upto 0xffe0
//#define MAX_BLOCK_SIZE 0xffe0
#define MAX_BLOCK_SIZE 0xffd0

#define MAX_FNAME_LEN 32
#define DEBUG
#ifdef DEBUG
    #define XDEBUG(...) printf(__VA_ARGS__)
#else
    #define XDEBUG(...)  /**/
#endif


#define SUCCESS 1
#define FAIL -1

#define POLLTIMEOUT 3000
#define MAX_PKT_SZ 0xff

/* DATA Structures/typedefs */
typedef enum {
  CLI_INIT, CLI_CONNECT, CLI_ACK,CLI_BLOCK_REQ, CLI_PAUSE, CLI_ERR //TBD: possibly, can enumerate more client errors?
}tCliMsgType;

#ifndef MAX_RES_LEN
#define MAX_RES_LEN 16
#endif
typedef struct {
 int cliPort; //Assumption: this is a randomly selected port, used as client Identifier
              //TBD: add more client uniqueness such as SA or a random Seq (3 way handshake etc)
 int type;
 int seqAck; 
 int len;
 char resName[MAX_RES_LEN];
}tCliMsg;

typedef enum {
  SRV_INIT, SRV_DATA, SRV_NORES, SRV_END, SRV_BUSY
}tSrvMsgType;

typedef struct {
 int type;
 int seq;
 int blockSize;
 long size; //total size 
} tSrvMsgHdr;

typedef struct {
  tSrvMsgHdr header;
  char data[MAX_BLOCK_SIZE];
}tSrvMsgData;

// function protos
int initClientSocket(int srvAddr, int port, struct sockaddr_in *si_me);
int initSrvSocket(int port, struct sockaddr_in *si_me);
int getResourceInfo(char* reqFile);
void dumpDataPkt(tSrvMsgHdr *pmsg);
long getAvailableSpace(const char* path);
int skipAction(int frac);
int getIp(char* ipAddrStr);
