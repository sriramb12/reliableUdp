
Reliable UDP
   A udp client/server  application to transfer files 

how to use: 
  Run the server in a termial
   ./srv
  Run the client in another terminal to fetch a file from server loction

   ./cli <filename>
  Ex: cli 4k


How to build:
  git clone the code to a local linux system 
  cd reliableUDP
  make

Prerquisites:
  standard development tools:
  gcc, make utilities etc

  
Approach :
   Simple and straightforward implementation
   No buffering of out of sequence (OOS) packets so the OOS pkts will be discarded
   single client at a time . No concurrent transfer
   A predifined packet size of upto 0xffe0 [0xffff is UDP datagram limit] is allowed
   Retries at every stage for ack 

  server listens on 1111
  client ---INIT---> server 
              server --->INIT ACK---> client
                      client --->CONNECT---> server
                                      |server --->DATA---> client|
                                      |server <--ACK <---- client|
             
    INIT packet will have client's (random) port for server to use for the sesion
                                 request for specific resource (eg file name) 
    INIT-ACK packet will have resource info (file/resource size, block size )
    
    if resource not found server will send error msg to client
    client will send CONNECT msg and server starts sending DATA packets (size == MAX_BLOCK_SIZE)
    Each DATA pkt will have a 'sequence number' (starting from 0)
    Server will wait for ACK from client before sending next DATA pkt.
     So,  each packet is ACKed by cleint
    Last DATA pkt may be smaller (remainder of file) than the other DATA pkts
  
TODO:
   FSM
   Testing 

