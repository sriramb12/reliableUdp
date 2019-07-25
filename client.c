/*
 *  file: client.c 
 *  Functionality: implements a standalone network stream client(consumer)
 *     using reliable udp 
 *  how to run:
 *    ./cli <filename>
 *  Limitations:  
 *     
 */

#include "common.h"
#include "client.h"

//Globals
struct sockaddr_in clisock, svrsock;
char* srvMsgTypeToStr[] =  { "Srv Init", "Data", "No such resource", "End", "Busy"};
                            //SRV_INIT,  SRV_DATA, SRV_NORES,       SRV_END,SRV_BUSY
tCliMsgType lastMsgType;
tClientContext cliContext; 

//Creates a local file (before proceeding to get from server) with a prefix '.copy'
int createDataFile()
{
    strcpy(cliContext.filename, cliContext.resName);
    strcpy(cliContext.filename+strlen(cliContext.resName), ".copy");
    XDEBUG("dest file : %s\n", cliContext.filename);
    //exit(0);
    int fd = open(cliContext.filename, O_CREAT| O_RDWR, 0644);
    if(fd == -1)
    {
     perror("Exiting: Create local file failed");
     exit(0);
    }
    return fd;
}

//creates write fd , checks for avbl disk space and connects to server
void init(char** av)
{
  char* fname = av[1];
  strncpy(cliContext.resName, fname, MAX_RES_LEN-1); 
  cliContext.writeFd = createDataFile();
  //TBD: find our available disk space and send to server
  cliContext.avblDiskSpace = getAvailableSpace(STORE_PATH);
  int ipInt = getIp(av[2]);
  if(!ipInt)
  {
      fprintf(stderr, "Invalid server address %s\n", av[2]);
      exit(0);
  }
  cliContext.srvAddr = ipInt;
  if(av[2] && ipInt) 
  {
     printf("Connecting to Server %s\n", av[2]);
  }  
  else
     printf("Connecting to Local Server %s\n", av[2]);
     
  printf("running as client fetch file:%s\n", cliContext.resName);
  srand(time(0)); 
  cliContext.cliPort = (rand())%10000+ 10000; 
  XDEBUG("Client Will be listening on %d(%x)\n", cliContext.cliPort, cliContext.cliPort);
  cliContext.srvfd = initClientSocket(cliContext.srvAddr, SRV_PORT, &svrsock);
  cliContext.clifd = initSrvSocket(cliContext.cliPort, &clisock);

  return;
}

// Process the INIT_ACK from server
// Check the resource details and see if there is enough space
// If not send error to server
int processInitAck()
{
   tSrvMsgData* pSrvMsg = &cliContext.srvMsgBuf; 
   if(myState != ST_CLI_INIT_ACK)
   {
      XDEBUG("Discarding pkt, Init state\n");
      return 0;
   }
   if(pSrvMsg->header.size >= cliContext.avblDiskSpace)
   {
       //  TBD: Tell server about our capacity.
       //sendMsgToSvr(CLI_ERR, 0);
       printf("Insufficient disk space : Capacity %d file sz %ld\n", cliContext.avblDiskSpace, pSrvMsg->header.size);
       sendMsgToSvr(CLI_ERR);
       exit(0);
   }
   myState = ST_CLI_DATA;

   cliContext.size = pSrvMsg->header.size;
   cliContext.blockCount = pSrvMsg->header.size/pSrvMsg->header.blockSize +1;

   XDEBUG("Init file size %ld, blocksize %d, blockCount %d\n",
    pSrvMsg->header.size, pSrvMsg->header.blockSize, cliContext.blockCount);
   
   
   return sendMsgToSvr(CLI_CONNECT);
}

//Only in order packets will be saved for now.  
int writeDataToDisk()
{
      tSrvMsgData* pSrvMsg = &cliContext.srvMsgBuf; 

      //save to disk
      int toWrite = pSrvMsg->header.size - sizeof(pSrvMsg->header); 
      //XDEBUG("Writing to disk %d bytes, fd=%d\n", toWrite, cliContext.writeFd); 
      int writtenBytes = write(cliContext.writeFd, pSrvMsg->data, toWrite);
      if(writtenBytes != toWrite)
      {
          perror("write to disk failed");
          //TBD: neeto handle partial? Transient failure? 
          //disk space could be an issue 
          relUdpStats.diskWriteFails++;
          if(relUdpStats.diskWriteFails > MAX_DISK_WRITE_FAILS)
          {
            return 0;
          }
      }
      return writtenBytes;
}


//
int processData()
{
   tSrvMsgData* pSrvMsg = &cliContext.srvMsgBuf; 
   if(myState != ST_CLI_DATA)
   {
      XDEBUG("Discarding data packet\n");
      sendMsgToSvr(CLI_CONNECT);
      return 0;
   }
   XDEBUG("expected:%d recvd: %d\n", cliContext.seq, pSrvMsg->header.seq);
   if(cliContext.seq > pSrvMsg->header.seq)
   {
        XDEBUG("Old (dup) pkt?\n");
        relUdpStats.duplicatePkts++;
        return 1;
   }
   if(cliContext.seq == pSrvMsg->header.seq)
   {
      cliContext.rxBytes += pSrvMsg->header.size-sizeof(tSrvMsgHdr);
      cliContext.rxBlocks++;
      XDEBUG("Transfer %d bytes, %d blocks \n", cliContext.rxBytes, cliContext.rxBlocks);
    
      int written = writeDataToDisk();
      if(written)
      {
         if(sendMsgToSvr(CLI_ACK))
         {
           if(cliContext.rxBytes == cliContext.size)
           {
              printf("Transfer complete %d bytes, %d blocks \n", cliContext.rxBytes, cliContext.rxBlocks);
              exit(0);
           }
          //increment seq
          cliContext.seq = pSrvMsg->header.seq+1;
          return 1;
         }
         XDEBUG("Failed to send ack to server\n");
         relUdpStats.sendFails++;
      }
      
      //else , server will retry
   }
   
   return 1;
   //TBD : need to see if we actully need to buffer it?
   // important consideration: HUGE hlock size
}

void dumpDataPkt(tSrvMsgHdr *header)
{
   XDEBUG("pkt header:");
   XDEBUG("Type %s ", srvMsgTypeToStr[header->type]); 
   XDEBUG("Seq %d ", header->seq);
   XDEBUG("BlockSz %d ", header->blockSize);
   XDEBUG("Size %ld ", header->size);
   XDEBUG("\n");
}

//
int handleServMsg()
{
      tSrvMsgData* pSrvMsg = &cliContext.srvMsgBuf; 
      tSrvMsgType msgType = pSrvMsg->header.type;
      dumpDataPkt(&pSrvMsg->header);
      switch(msgType)
      {
         case SRV_INIT:
           return processInitAck();
         case SRV_BUSY:
         case SRV_NORES:
         case SRV_END:
           printf("Server Error: %s, exiting\n", srvMsgTypeToStr[msgType]); 
           exit(0);
         case SRV_DATA:
           return processData();
         default:
           XDEBUG("ignore unknown type(%d) of pkt\n", msgType);
           return 1;
      }
    return 0;
}

//
int process()
{
  socklen_t addr_size = sizeof(struct sockaddr_in);
  int dataPending = 1;
  printf("Send request for file: %s\n", cliContext.resName);
  sendMsgToSvr(CLI_INIT);
  //long numfds = 1, retval, timeout = SERVER_CONN_TIMEOUT;
  long numfds = 1, retval, timeout = -1;

  struct pollfd poll_list[1];
  poll_list[0].fd = cliContext.clifd;
  poll_list[0].events = POLLIN;
  int clientRetries = 0;
  
  while(dataPending)
  {
      retval = poll(poll_list, numfds, timeout);
      if(retval < 0)
      {
            fprintf(stderr,"Error while polling: %s\n",strerror(errno));
            return 0;
      }
      if(!retval)
      {
          if( clientRetries > MAX_CLIENT_RETRIES)
          {
            fprintf(stderr,"Server not responding\n");
            //remove local copy 
            remove(cliContext.filename); 
            exit(0);
          }
          if(myState == ST_CLI_INIT_ACK)
          {
               sendMsgToSvr(CLI_CONNECT);
          }
          sendMsgToSvr(lastMsgType);
          XDEBUG("Timeout\n");
          clientRetries++;
      }
      if(retval > 0)
      {
         int len = recvfrom(cliContext.clifd, (char*)(&cliContext.srvMsgBuf), MAX_BLOCK_SIZE, 0,
                           (struct sockaddr*)&clisock, &addr_size);
         if(len <=0 )
         {
            perror("client:Recvfrom");
            exit(0);
         }
         clientRetries = 0;
         //XDEBUG("msg from: %s len %d\n",inet_ntoa(clisock.sin_addr), len);
         dataPending = handleServMsg();
      }
 
  }
  return 1;
}

//
int runClient()
{
  process();
  return 0;
}

//
int validateDataMsg(tSrvMsgHdr *pSrvMsg)
{
   //TBD
   return 1;
}

//
int main(int argc, char **av)
{
  if (argc <=1 || argc>=4)
  {
     printf("Usage: %s <file> [serverIp]\n", av[0]);
     exit(0);
  }
  init(av);
  runClient();
  return 0;
}

//  CLI_INIT, CLI_CONNECT, CLI_ACK,CLI_BLOCK_REQ, CLI_PAUSE, CLI_ERR //TBD: possibly, can enumerate more client errors?

char* cliMsgTypeToStr[] = {"Init", "connect", "ack", "block req", "pause", "err"};

//Send message (of given type) to server 
int sendMsgToSvr(tCliMsgType type)
{
  tCliMsg msg;
  socklen_t addr_size = sizeof(struct sockaddr_in);
  msg.type = type;
  msg.cliPort = cliContext.cliPort;

  //XDEBUG("client msg type %s\n", cliMsgTypeToStr[type]);  
  if(type == CLI_INIT) 
  {
     strncpy(msg.resName, cliContext.resName, MAX_RES_LEN-1);
     msg.len = cliContext.avblDiskSpace; 
  }
  if(type == CLI_ACK) 
  {
     //msg.seqAck = cliContext.seq;
     msg.seqAck = cliContext.srvMsgBuf.header.seq;
     XDEBUG("client seq %d\n", msg.seqAck);  
  }
  lastMsgType = type;
  int len =  sendto(cliContext.srvfd, (char*)&msg, sizeof(tCliMsg), 0, (struct sockaddr*)&svrsock, addr_size);
  if (len < 0)
  {
       perror("Sendto server failed");
       return 0;
  }
  return 1;
}
