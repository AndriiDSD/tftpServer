
//Andrii Hlyvko. Network Centric Programming
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//TFTP Protocol offsets
#define OPCODE_OFFSET 0
#define BLOCK_OFFSET 2
#define ERROR_CODE_OFFSET 2
#define MSG_OFFSET 4
#define DATA_OFFSET 4

#define DGRM_LEN 516
#define DATA_LEN 512

/*
  This method converts a string to an integer.
  @param char * str - the string to convert
  @return int - the converted string
 */
int strToInt(char *);

/*
  This method creates a socket and binds it to a local port.
 */
int createSocket(int port);

/*
  This method finds the maximum of an array of integers.
 */
int maxArr(int arr[],int size);

/*
  This method sends an error packet to the specified address. 
 */
void sendErrorPacket(int serverFd,const struct sockaddr* clientAddr,ssize_t clentS,int code);//sends an error packet to the client

/*
  This method handles the initila client request from the listening socket.
  It creates a TID opens the file and fills in the client address. Then it
  sends the first data packet from the new TID.
 */
void handleServerRequest();

/*
  This method responds to ACK packets with the next data packet. If it was
  the last data packet then the file is closed and the client socket that
  was used becomes available for next request.
 */
void handleClientRequest(int i);

//global vars
int isfreeSocket[20];//true/false array to identify available sockets
int socketFds[20];   //array of sockets 
int requestedFiles[20];//array of requested file descriptors
//struct sockaddr_in clients[20];//array of client addresses
fd_set readSet;                

int main(int argc, char** argv) {

  int maxfdp;
  // Check arguments
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }
  int port=strToInt(argv[1]); //proxy port
  int i;
  for(i=0;i<sizeof(socketFds)-1;i++)
    {
      socketFds[i]=createSocket(port+i);
    }

  for(i=1;i<20;i++)//all are available initially
    {
      isfreeSocket[i]=1;
    }

  FD_ZERO(&readSet);
  maxfdp=max(socketFds,20)+1;//find the max socket descriptor
  for(;;)
    {
      FD_SET(socketFds[0],&readSet);
      for(i=1;i<20;i++)
	{
	  if(isfreeSocket[i])//if socket is free clear it
	    {
	      FD_CLR(socketFds[i],&readSet);//if addr==null clear from set
	    }
	  else//has an addr so add to set
	    {
	      FD_SET(socketFds[i],&readSet);
	    }
	}
      if((select(maxfdp,&readSet,NULL,NULL,NULL))<0){
	if(errno==EINTR)
	  continue;
	else
	  {
	    printf("select error\n");
	    exit(1);
	  }
      }
      if(FD_ISSET(socketFds[0],&readSet))//the listening socket is set
	{
	  handleServerRequest();
	  //sleep(1);
	}
      for(i=1;i<20;i++)//check which clients are set
	{
	  if(FD_ISSET(socketFds[i],&readSet))
	    {
	      handleClientRequest(i);
	      //sleep(1);
	    }
	}
    }
  
  close(socketFds[0]);
  return 0;
  
}

void sendErrorPacket(int serverFd,const struct sockaddr*client,ssize_t clientSize,int errorCode)
{
  char *msg;
  
  switch(errorCode)
    {
    case 7:msg="No such user";break;
    case 6:msg="File already exists";break;
    case 5:msg="Unknown TID";break;
    case 4:msg="Illegal TFTP Operation";break;
    case 3:msg="Full Disc";break;
    case 2:msg="Access Violation";break;
    case 1:msg="File not found";break;
    default:msg="Not Defined";
    }
  if(errorCode<0||errorCode>7)
    errorCode=0;
  uint8_t errorPacket[60];     //error packet
  *((uint16_t*)(errorPacket+OPCODE_OFFSET))=htons(5);
  *((uint16_t*)(errorPacket+ERROR_CODE_OFFSET))=htons(errorCode);
  strcpy((char*)(errorPacket+MSG_OFFSET),msg);
  //printf("string: %s\n",(char*)(errorPacket+MSG_OFFSET));
  sendto(serverFd,errorPacket,sizeof(errorPacket),0,client,clientSize);
  
}


/*
 *This method converts a string to int. It is used to 
 *convert the proxy port argument.
 * @args char *st - the string to be converted.
 */
int strToInt(char *st)
{
  int x=0;
  if(st==NULL)
    return -1;
  if(*st=='\0')
    return -1;
  char *ch;
  for(ch=st;*ch!='\0';ch++)
    {
      if(!isdigit(*ch))
	x=-1;
    }
  if(x!=-1)
    x=(int)atoi(st);
  return x;
}

int createSocket(int port) {
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);  
  if(sockfd<0)
    {
      perror("Could not open socket\n");
      exit(0);
    }
  int yes=1;
  if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1)//reuse address
    {
      perror("setsockopt\n");//failed to set socket options
      exit(1);
    }
  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr=INADDR_ANY;
  dest.sin_port = htons(port);
  
  if( bind(sockfd, (struct sockaddr*) &dest, sizeof(dest)) < 0) {
    perror("bind");
    exit(1);
  }
  return sockfd;
}
int max(int arr[],int size)
{
  if(size<0)
    return -1;
  if(size==0)
    return arr[0];
  int max=arr[0];
  int i=0;
  
  int n;
  for(n=0; n<size;n++)
    {
      if(arr[n]>max)
	{
	  max=arr[n];
	  i=n;
	}
    }

  return max;
  
}

void handleServerRequest()
{
  socklen_t clientSize=(socklen_t)sizeof(struct sockaddr_in);
  int bytesRecieved=0;
  char message[100];
  uint16_t opcode;
  struct sockaddr_in clientAddr;
  
  
  bytesRecieved=recvfrom(socketFds[0],message,sizeof(message),0,(struct sockaddr*)&clientAddr,
			       &clientSize);//recieve packet
  int available=-1;
  int i;
  for(i=1;i<20;i++)//check for avilable sockets
    {
      if(isfreeSocket[i])
	{
	  available=i;
	  break;
	}
      else
	{
	  continue;
	}
    }
  if(available==-1)//no free sockets exists
    return;

  isfreeSocket[available]=0;//not available now

  memcpy(&opcode,message,sizeof(uint16_t));//2 byte opcode
  
  opcode=ntohs(opcode);
  //printf("opcode=%d\n",opcode);
  if(opcode==1)//RRQ
    {
      //printf("%s\n",message);
      printf("RRQ");
      char *fileName=message+2;//print the name of the file
      printf(" %s",fileName);//
      char *mode=fileName;
      for(;*mode!='\0';mode++)//get mode
	{
	  //printf("%c\n",(int)*mode);
	}
      mode=mode+1; 
      printf(" %s from ",mode);  //print mode
      
      //get and print the ip and port number of client
      unsigned long client;//
      unsigned char a, b, c, d;//ip chars
      char ipAddress[20];
      client = ntohl(clientAddr.sin_addr.s_addr);
      a = client >> 24;
      b = (client >> 16) & 0xff;
      c = (client >> 8) & 0xff;
      d = client & 0xff;
      sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
      printf("%s",ipAddress);
      int clientPort=ntohs(clientAddr.sin_port);
      printf("%d\n",clientPort);
      
      int fileFd=open(fileName,O_RDONLY); 
      if(fileFd==-1)//check if open failed
	{
	  fprintf(stderr,"./tftp.c: %s\n",strerror(errno));
	  sendErrorPacket(socketFds[available],(const struct sockaddr *)&clientAddr,clientSize,1);//file not found
	  isfreeSocket[available]=1;
	  return;
	}
      requestedFiles[available]=fileFd;
      //read in the first data packet and send from new socket
      int blockNum=1;
      size_t readB=0;
      uint8_t dataPacket[DGRM_LEN];
      
      bzero(dataPacket,sizeof(dataPacket));
      readB=read(fileFd,(dataPacket+DATA_OFFSET),DATA_LEN);
      //printf("read %d\n",(int)readB);
      *((uint16_t*)(dataPacket+OPCODE_OFFSET))=htons(3);//data opcode is 3
      *((uint16_t*)(dataPacket+BLOCK_OFFSET))=htons(blockNum); //start with block 1
      if(readB==-1)
	{
	  fprintf(stderr,"./tftp %s\n",strerror(errno));
	  exit(1);
	}
      if(readB<512)
	{
	  dataPacket[readB+DATA_OFFSET]=EOF;
	}
      if(readB==512)
	{
	sendto(socketFds[available],dataPacket,sizeof(dataPacket),0,(const struct sockaddr *)&clientAddr,clientSize);
	printf("sent DATA <block=%d, %d bytes> to ",blockNum,(int)readB);
	sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
	printf("%s",ipAddress);
	printf("%d\n",clientPort);
	}
      else //first packet contains the whole fire
	{
	  sendto(socketFds[available],dataPacket,readB+4,0,(const struct sockaddr *)&clientAddr,clientSize);
	  close(fileFd);
	  isfreeSocket[available]=1;//now available
	  FD_CLR(socketFds[available],&readSet);
	  printf("sent DATA <block=%d, %d bytes> to ",blockNum,(int)readB);
	  sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
	  printf("%s",ipAddress);
	  printf("%d\n",clientPort);
	}        
    }
  else if(opcode==2)//WRQ
    {
      printf("WRQ");
      char *fileName=message+2;//print the name of the file
      printf(" %s\n",fileName);//
      char *mode=fileName;
      for(;*mode!='\0';mode++)
	{
	  //printf("%c\n",(int)*mode);
	}
      mode=mode+1;
      printf(" %s from ",mode);
      
      //get and print the ip and port number of client
      unsigned long client;//
      unsigned char a, b, c, d;//ip chars
      char ipAddress[20];
      client = ntohl(clientAddr.sin_addr.s_addr);
      a = client >> 24;
      b = (client >> 16) & 0xff;
      c = (client >> 8) & 0xff;
      d = client & 0xff;
      sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
      printf("%s",ipAddress);
      int clientPort=ntohs(clientAddr.sin_port);
      printf("%d\n",clientPort);
      sendErrorPacket(socketFds[available],(const struct sockaddr *)&clientAddr,clientSize,1);
    }
  else if(opcode==3)//DATA
    {
      printf("DATA\n");
    }
  else if(opcode==4)//ACK
    { 
      printf("ACK Block:");
      int blockNum=0;
      memcpy(&blockNum,message+BLOCK_OFFSET,sizeof(uint16_t));//2 byte opcode
      blockNum=ntohs(blockNum);
      printf("%d\n",blockNum);
    }
  else//ERROR
    {
      printf("ERROR\n");
      
    }
}

void handleClientRequest(int clientNum)
{
  socklen_t clientSize=(socklen_t)sizeof(struct sockaddr_in);
  int bytesRecieved=0;
  char message[100];
  uint16_t opcode;
  struct sockaddr_in clientAddr;
  int blockNum=0;
  
  unsigned long client;//for printing client info
  unsigned char a, b, c, d;//ip chars
  char ipAddress[20];
  int clientPort;

  bytesRecieved=recvfrom(socketFds[clientNum],message,sizeof(message),0,(struct sockaddr*)&clientAddr,
			       &clientSize);//recieve packet
  client = ntohl(clientAddr.sin_addr.s_addr);
  a = client >> 24;
  b = (client >> 16) & 0xff;
  c = (client >> 8) & 0xff;
  d = client & 0xff;
  clientPort=ntohs(clientAddr.sin_port);
  memcpy(&opcode,message,sizeof(uint16_t));//2 byte opcode
  
  opcode=ntohs(opcode);
    if(opcode==4)//ACK
    { 
      printf("recieved ACK <block=");
      memcpy(&blockNum,message+BLOCK_OFFSET,sizeof(uint16_t));//2 byte opcode
      blockNum=ntohs(blockNum);
      printf("%d> from ",blockNum);    
      sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
      printf("%s",ipAddress);
      printf("%d\n",clientPort);
    }
  else//ERROR
    {
      printf("ERROR\n");
      FD_CLR(socketFds[clientNum],&readSet);//clear from set
      isfreeSocket[clientNum]=1;//make socket free
      close(requestedFiles[clientNum]);//close the file
      return;
    }
    
    //send the next block of data
    blockNum++;
    size_t readB=0;
    uint8_t dataPacket[DGRM_LEN];
    
    bzero(dataPacket,sizeof(dataPacket));
    readB=read(requestedFiles[clientNum],(dataPacket+DATA_OFFSET),DATA_LEN);
    //printf("read %d\n",(int)readB);
    *((uint16_t*)(dataPacket+OPCODE_OFFSET))=htons(3);//data opcode is 3
    *((uint16_t*)(dataPacket+BLOCK_OFFSET))=htons(blockNum); //start with block 1
    if(readB==-1)
      {
	fprintf(stderr,"./tftp %s\n",strerror(errno));
	exit(1);
      }
    if(readB<512)
      {
	dataPacket[readB+DATA_OFFSET]=EOF;
	sendto(socketFds[clientNum],dataPacket,readB+4,0,(const struct sockaddr *)&clientAddr,clientSize);
	close(requestedFiles[clientNum]);
	isfreeSocket[clientNum]=1;//now available
	FD_CLR(socketFds[clientNum],&readSet);
	printf("sent DATA <block=%d, %d bytes> to ",blockNum,(int)readB);
	sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
	printf("%s",ipAddress);
	printf("%d\n",clientPort);
	
      }
    else if(readB==512)
      {
	sendto(socketFds[clientNum],dataPacket,sizeof(dataPacket),0,(const struct sockaddr *)&clientAddr,clientSize);
	printf("sent DATA <block=%d, %d bytes> to ",blockNum,(int)readB);
	sprintf(ipAddress, "%d.%d.%d.%d:", a, b, c, d);
	printf("%s",ipAddress);
	printf("%d\n",clientPort);
      }
    
}

// Appendix:
//            2 bytes     string    1 byte     string   1 byte
//          ------------------------------------------------
//           | Opcode |  Filename  |   0  |    Mode    |   0  |
//           ------------------------------------------------
//
//                     Figure 5-1: RRQ/WRQ packet

      //      2 bytes  2 bytes        string    1 byte
      //    ----------------------------------------
      //ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
      //     ----------------------------------------
      //

//                 2 bytes     2 bytes      n bytes
//                 ----------------------------------
//                | Opcode |   Block #  |   Data     |
//                 ----------------------------------
//
//                      Figure 5-2: DATA packet


//                       2 bytes     2 bytes
//                       ---------------------
//                      | Opcode |   Block #  |
//                       ---------------------
//
//                       Figure 5-3: ACK packet

//Error Codes

// Value     Meaning

// 0         Not defined, see error message (if any).
// 1         File not found.
// 2         Access violation.
// 3         Disk full or allocation exceeded.
// 4         Illegal TFTP operation.
// 5         Unknown transfer ID.
// 6         File already exists.
// 7         No such user.
