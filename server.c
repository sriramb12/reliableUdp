/*
 *  file: server.c 
 *  Functionality: implements a standalone network data (can be file for now) server(producer)
 *     implements reliable udp 
 *  Limitations:
 *     Server is not concurrent, one client at a time
 *     No buffering of out of sequence data, so it discards any such
 *     Some system error conditions need to be handled
 *     maximum packet size tested 0xfff0 
 *  Tuning/performance Considerations:
 *    o Message size (set MAX_BLOCK_SIZE) 
 *    o <>
 *  ToDos:
 *    implement concurrent server (using threads ?)
 *    test for robustness build robust recovery by playing with random pkt drops
 *    Implement Finite state machine for program robust and maintainable code
 *    Test across internet
 */

#include "common.h"
#include "server.h"

//Debugging
struct tSrvErrors{
  int seqMismatch;
  int openFailed;
  int fileReadFailed;
  int sendFailed;
  int recvFailed;
  int connectTimeouts;
  int dataTimeouts;
} srvErrors; 

//TBD: need to create a client specific session
//global data (for now)
tCliMsg cliMsg;
struct sockaddr_in cliAddr;
tSrvMsgHdr srvMsgHdr;
tSrvMsgData srvMsgData;
socklen_t addr_size = sizeof(struct sockaddr_in);
int seq = 0;
int clifd = 0;
int resfd=-1;
int readBlockSize;
int startTime;
int rdBytes, rdBlocks;
int end = 0;

//context for message retry mechanism
tSrvMsgHdr lastHdr;
tSrvMsgData lastData;
tSrvMsgType lastMsgType;
int retries;
int payloadLen;


int lastServerMsgSent = 0; 
#define SRV_TIMEOUT 200 // after which server will reset to listening , resetting current client 

tSrvState srvState = ST_SRV_LISTEN;
int main(int argc, char **argv){
  struct sockaddr_in me;
  int srvfd = initSrvSocket(SRV_PORT, &me);

  if(srvfd <0)
  {
    perror("");
    exit(0);
  }
  server(srvfd);
  return 0;
}

// Reset Server to LISTEN when transfer is done
void reset()
{
  srvState = ST_SRV_LISTEN;
  close(clifd);
  close(resfd);
  clifd = resfd = -1;
  cliMsg.cliPort = 0;
  bzero(&srvErrors, sizeof(srvErrors));
  seq = 0;
  end = 0;
  retries = 0;
}

int retryLastMsg()
{
  if(srvState == ST_SRV_LISTEN)
     return 0;
  
  if(retries > MAX_SRV_RETRIES)
  {
         XDEBUG("Client not acknowledging . Server Resetting\n");
         sendClientMsg(SRV_END);
         reset();
         return 1;
  }
  if (lastMsgType == SRV_DATA)
  {
     retries++;
     srvErrors.dataTimeouts++;
     XDEBUG("Resending last data packet\n");
     return sendData(1);
  }
  srvErrors.connectTimeouts++;
  return sendClientMsg(lastMsgType);
}

//Reads from Disk as per the MAX_BLOCK_SIZE at a time.
int readFromDisk()
{
   int len = read(resfd, srvMsgData.data, readBlockSize);
   XDEBUG("Read %d bytes from file\n", len);
   rdBytes += len;
   rdBlocks++;
   if(len < readBlockSize)
   {
       end = 1;
   }
   if(len < 0)
   {
     perror("read block failed");
     srvErrors.fileReadFailed++;
     return 0;
   }
   return len;
}

// Sends Data to client. It also resends when ACK is not recvd. makes use of
// previous data read from file
int sendData(int retry)
{
   int len;
   if(!retry)
   {
     payloadLen = sizeof(tSrvMsgHdr) + readFromDisk();
   }
   srvMsgData.header.type = SRV_DATA;
   srvMsgData.header.seq = seq;
   srvMsgData.header.size = payloadLen; 
   srvMsgData.header.blockSize = readBlockSize; 
   
   cliAddr.sin_port = htons(cliMsg.cliPort);
  
   //if(!skipAction(1000)) //simulate server data pkt loss
   {
      len =  sendto(clifd, (char*)&srvMsgData, payloadLen, 0, (struct sockaddr*)&cliAddr, addr_size);
      if(len >= payloadLen)
         XDEBUG("len %d payloadLen %d\n", len, payloadLen);
      if(len <= 0)
      {
          perror("Sendto:");
          return sendClientMsg(SRV_END);
      }
   }
   XDEBUG("Sending data seq %d len %d port %x\n", seq, payloadLen, cliMsg.cliPort); 
   XDEBUG("to %s: UDP port %u len %d\n", 
       inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port), len);
   lastServerMsgSent = time(0);
   srvState = ST_SRV_DATA_ACK;
   lastMsgType = SRV_DATA;
   return 1;
}

char* srvMsgTypeToStr[] =  { "Srv Init", "Data", "No such resource", "End", "Busy"};
  //SRV_INIT, SRV_DATA, SRV_NORES, SRV_END, SRV_BUSY
int sendClientMsg(tSrvMsgType type)
{
  socklen_t addr_size = sizeof(struct sockaddr_in);
  srvMsgHdr.type = type;
  lastMsgType = type;
  XDEBUG("Server msgType %s\n", srvMsgTypeToStr[type]);
  if(clifd <=0)
  {
     clifd = initClientSocket(cliAddr.sin_addr.s_addr, cliAddr.sin_port, &cliAddr);
     startTime = time(0);
  }
  int len =  sendto(clifd, (char*)&srvMsgHdr, sizeof(tSrvMsgHdr), 0, (struct sockaddr*)&cliAddr, addr_size);
  XDEBUG("to %s: UDP port %u len %d\n", 
       inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port), len);
  lastServerMsgSent = time(0);
  if(len < 0)
     perror("sendClientMsg:sendto");
  return -1;
}

int processDataAck()
{
  retries = 0;
  if (srvState != ST_SRV_DATA_ACK)
  {
    XDEBUG("187 Server Busy\n");
    sendClientMsg(SRV_END);
    reset();
    return 0;
  }
  if(cliMsg.seqAck > seq)
  {
    XDEBUG("spurious pkt? sequence mismatch Exp %d Rx %d\n", seq, cliMsg.seqAck);
    srvErrors.seqMismatch++;
    return 0; 
  }
  if(cliMsg.seqAck < seq)
  {
    XDEBUG("old seq mismatch Exp %d Rx %d\n", seq, cliMsg.seqAck);
    srvErrors.seqMismatch++;
  }
  else 
      seq +=1;
  XDEBUG("Rx sequence  %d\n", cliMsg.seqAck);
  if(end)
  {
     dumpSummary();
     reset();
     return 1;
  }
  return sendData(0); 
}
int processConnect()
{
  if (srvState != ST_SRV_INIT_ACK)
  {
    XDEBUG("216 Server Busy\n");
    sendClientMsg(SRV_END);
    reset();
    return -1;
  }
  
  retries = 0;
  return sendData(0);
}

int processInit()
{
  srand(time(0)); 
  readBlockSize = MAX_BLOCK_SIZE - sizeof(tSrvMsgHdr);
  if (srvState != ST_SRV_LISTEN)
  {
    XDEBUG("230 Server Busy\n");
    sendClientMsg(SRV_END);
    return -1;
  }
  int fileSz = getResourceInfo(cliMsg.resName);
  resfd = open(cliMsg.resName, O_RDONLY);
  srvMsgHdr.blockSize = readBlockSize;  
  srvMsgHdr.size = fileSz;
  printf("Process transfer req: %s file sz %d blockSize %d client port:%d\n",
         cliMsg.resName, fileSz, readBlockSize, cliMsg.cliPort);
  if(fileSz >0)
  {
     srvState = ST_SRV_INIT_ACK;
     return sendClientMsg(SRV_INIT);
  }
  return sendClientMsg(SRV_NORES);
}

//  CLI_INIT, CLI_CONNECT, CLI_ACK,CLI_BLOCK_REQ, CLI_PAUSE, CLI_ERR //TBD: possibly, can enumerate more client errors?
char* cliMsgTypeToStr[] =  { "Init", "Connect", "Ack", "BlockReq", "Pause", "Abort"};
int processClientMsg()
{
  if (cliMsg.type > CLI_ERR)
  {
     XDEBUG("discard unknown client msg %d\n", cliMsg.type);
     return 0;
  }
  //XDEBUG("process client msg %s\n", cliMsgTypeToStr[cliMsg.type]);
  switch(cliMsg.type)
  {
     case CLI_INIT:
       return processInit();
     case CLI_CONNECT:
       return processConnect();
     break;
     case CLI_ACK:
       return processDataAck();
     break;
     case CLI_ERR:
        reset();
        return 1;
   //tbd: cleanup
     break;
  }
  return 0;
}

int validateCliMsg(int len)
{
   if(len != sizeof(tCliMsg) || cliMsg.cliPort > 20000)
   {
        XDEBUG("Invalid pkt Rx %d port %d \n", len, cliMsg.cliPort);
        return 0;
   }
   return 1;
}

void recvClientMsg(int sockfd)
{
   //XDEBUG("RX client msg\n");
   socklen_t addrLen = sizeof(cliAddr);
   int len = recvfrom(sockfd, (char*)&cliMsg, MAX_PKT_SZ, 0, (struct sockaddr*)&cliAddr, &addrLen);
    
   if(len <=0 )
   {
      perror("Recvfrom");
   }
   cliAddr.sin_port = (cliMsg.cliPort);
   if(!validateCliMsg(len))
     return; 
   XDEBUG("from %s: UDP %x len:%d\n", inet_ntoa(cliAddr.sin_addr), cliMsg.cliPort, len);
   processClientMsg();
}


void server(int fd)
{
  int retval, timeout = SRV_TIMEOUT;
  struct pollfd poll_list[1];

  printf("Reliable UDP Server started \n");
  reset();

  poll_list[0].fd = fd;
  poll_list[0].events = POLLIN;

  while(1)
  {
        retval = poll(poll_list,(unsigned long)2,timeout);
        /* Retval will always be greater than 0 or -1 in this case.
           Since we're doing it while blocking */
        if(retval < 0)
        {
            fprintf(stderr,"Error while polling: %s\n",strerror(errno));
            return ;
        }
        //XDEBUG("poll returned %d\n", retval);
        if(retval == 0)
        {
           if(time(0)- lastServerMsgSent > SRV_TIMEOUT && clifd > 0)
           {
              printf("Server timed out! listening for new connection");
              reset();
           }
           //Resend the last packet
           retryLastMsg();
           continue;
        }
        if(retval)
           recvClientMsg(fd);
  }
}

//Debugging
void dumpErrStats()
{
  int errors = 0;
  if(srvErrors.recvFailed)
        errors = 1, printf("recvFailed; %d\n", srvErrors.recvFailed);
  if(srvErrors.seqMismatch)
        errors = 1, printf("Seq Mismatch: %d\n", srvErrors.seqMismatch);
  if(srvErrors.openFailed)
        errors = 1, printf("openfail: %d\n", srvErrors.openFailed);
  if(srvErrors.fileReadFailed)
        errors = 1, printf("fileread Fails: %d\n", srvErrors.fileReadFailed);
  if(srvErrors.sendFailed)
        errors = 1, printf("sendFailed; %d\n", srvErrors.sendFailed);
  if(srvErrors.dataTimeouts)
        errors = 1, printf("dataAck timouts; %d\n", srvErrors.dataTimeouts);
  if(srvErrors.connectTimeouts)  //
        errors = 1, printf("connect timouts; %d\n", srvErrors.connectTimeouts);
  if(srvErrors.recvFailed)
        errors = 1, printf("recvFailed; %d\n", srvErrors.recvFailed);
//TBD: add more
  if (!errors)
     printf("Error-free transmission!\n");
}

void dumpStats()
{
     dumpErrStats();
     printf("Transfer completed in %ld seconds\n", time(0)- startTime);  
//TBD
}

void dumpSummary()
{
   dumpStats();
   dumpErrStats();
}

void dumpCumulativeSummary()
{
}
