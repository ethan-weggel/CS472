#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "du-proto.h"

static char _dpBuffer[DP_MAX_DGRAM_SZ];
static int  _debugMode = 1;

/*
* static dp_connp dpinit() is a static function that exists only in the context of this file (du-proto.c). 
* The goal of the function is to create a new instance of a dp_connection struct and initialize the values
* from random memory to useful starting values or zeroes. We start by declaring a variable 'dpsession' that is of
* type dp_connp which is typdef'd to represent a pointer to a dp_connection. This memory is pulled from the heap
* (however many bytes one dp_connection struct takes, denoted by sizseof(dp_connection)) and we save the pointer to this
* chunk of memory. We then initialize all bytes to the value of zero using bzero) and passing our pointer. Then for
* all fields we do the following:
*       dpsession->outSockAddr.isAddrInit = false [to say we have not intialized the address]
*       dpsession->inSockAddr.isAddrInit = false [to say we have not intialized the address]
*       dpsession->outSockAddr.len = sizeof(struct sockaddr_in) [to keep track of how big our 'outSock' address is]
*       dpsession->inSockAddr.len = sizeof(struct sockaddr_in) [to keep track of how big our 'inSock' address is]
*       dpsession->seqNum = 0 [to start our intial sequence number at zero when transmitting and receiving data]
*       dpsession->dbgMode = true [to set our debug mode to true]
*
* then we return this pointer so we can keep track of it and use it in other parts of our program with all of these fields 
* ready to use in a neutral state.
*/
static dp_connp dpinit(){
    dp_connp dpsession = malloc(sizeof(dp_connection));
    bzero(dpsession, sizeof(dp_connection));
    dpsession->outSockAddr.isAddrInit = false;
    dpsession->inSockAddr.isAddrInit = false;
    dpsession->outSockAddr.len = sizeof(struct sockaddr_in);
    dpsession->inSockAddr.len = sizeof(struct sockaddr_in);
    dpsession->seqNum = 0;
    dpsession->isConnected = false;
    dpsession->dbgMode = true;
    return dpsession;
}

/*
* void dpclose(dp_connp dpsession) simply takes an instance of dp_connp which is a pointer to a struct 'dp_connection'.
* This function then takes that pointer and frees the memory used to hold all fields and returns the memory to the heap
* so there are no memory leaks or resource problems in the program.
*/
void dpclose(dp_connp dpsession) {
    free(dpsession);
}

/*
* int  dpmaxdgram() simply returns a constant DP_MAX_BUFF_SZ. This is useful because DP_MAX_BUFF_SZ is used in other places
* to allocate memory to hold a datagram, but really, this also represents the maximum datagram we can store. It's a handy
* way to use one constant or 'magic number' and give it a second name within our program's context.
*/
int  dpmaxdgram(){
    return DP_MAX_BUFF_SZ;
}

/*
* dp_connp dpServerInit(int port) at its core takes a port number and returns a server socket that is receptive to clients connecting
* (i.e.) it starts listening for connections. It starts by declaring variables for a sockaddr_in struct where we can store
* the address we are going to use for our server. Then we call our dpinit() function so that we get a new dp_connection struct
* that has all empty/neutral values. If there was an error getting this memory or setting any of the fields then we print an error
* and return from the function. Since servaddr and sock are both pointers, we can set the address both of these variables point to.
* Therefore, we point our sock pointer at our emtpy dp_connp field called dpc->udp_sock. This is also an int and serves as a replacement
* for an int that is external to our struct to hold our socket file descriptor. We do something similar with our inSockAddr.addr. We 
* tell servaddr to now point to this place so that when we modify servaddr, we are really modifying the space that inSockAddr.arr 
* occupies. Next, we create our actual socket using IPv4 over UDP rather than TCP; if there is an error in this process, then we error and
* return. Now that we have our socket, we need some information about how to bind the socket, so we set the servaddr (really the dp_connp)
* and say we are using IPv4, we set the port number and use INADDR_ANY. Then as a safety measure that is really helpful for debugging,
* we setsockopt so that the os will give us the port number back quickly versus letting it free the port all on its own. Then we bind the 
* socket to this address and port using the bind() syscall and if there is an error we close the socket and error out. Finally we 
* set the field in dp_connp to say the in-address is initialized and set the length of it equal to the size of 'struct sockaddr_in'.
* Finally we return this partially populated dp_connp.
*/
dp_connp dpServerInit(int port) {
    struct sockaddr_in *servaddr;
    int *sock;
    int rc;

    dp_connp dpc = dpinit();
    if (dpc == NULL) {
        perror("drexel protocol create failure"); 
        return NULL;
    }

    sock = &(dpc->udp_sock);
    servaddr = &(dpc->inSockAddr.addr);
        

    // Creating socket file descriptor 
    if ( (*sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        return NULL;
    } 

    // Filling server information 
    servaddr->sin_family    = AF_INET; // IPv4 
    servaddr->sin_addr.s_addr = INADDR_ANY; 
    servaddr->sin_port = htons(port); 

    // Set socket options so that we dont have to wait for ports held by OS
    // if (setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0){
    //     perror("setsockopt(SO_REUSEADDR) failed");
    //     close(*sock);
    //     return NULL;
    // }
    if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
        close(*sock);
        return NULL;
    }
    if ( (rc = bind(*sock, (const struct sockaddr *)servaddr,  
            dpc->inSockAddr.len)) < 0 ) 
    { 
        perror("bind failed"); 
        close (*sock);
        return NULL;
    } 

    dpc->inSockAddr.isAddrInit = true;
    dpc->outSockAddr.len = sizeof(struct sockaddr_in);
    return dpc;
}

/*
* dp_connp dpClientInit(char *addr, int port) takes in a port and an address and returns a pointer to a dp_connection struct.
* In order to do this we declare variables for a 'struct sockaddr_in' which represents our server address, denoted 'servaddr';
* this is a pointer to this struct. We also declare a pointer to an integer that will represent our socket file descriptor. 
* Once we declare these, the dpinit() function is called to make a blank dp_connection and return us a pointer to it; if there is 
* an error, then we error out and return. Next we point sock at the dpc or dp_connp field udp_sock that is meant to hold an integer
* representing our socket filedescriptor. Then we point servaddr at 'dpc->outSockAddr.addr' so that when we modify servaddr, we are really
* modifying the corresponding field in our dp_connection. After this we create a socket by using the socket() syscall and we tell it to use
* IPv4 and UDP; if there is an error with our os syscall then we error out and return. Then after this we take our servaddr and set the address
* information to our server address (remember we changed where servaddr points). We set this up to be IPv4 using the port we pass in as
* a parameter and the address we pass in as a parameter. After this we set the length of the address in out dp_connection and say the out-address is
* initialized. We want the in-address to be the same as the out-address so we use memcpy() to copy the values of dcp->outSockAddr to dcp->inSockAddr
* for the length of the sockAddr. This also initializes the in-address. After this we return the pointer to the client dp_connection, denoted
* 'dp_connp'.
*/
dp_connp dpClientInit(char *addr, int port) {
    struct sockaddr_in *servaddr;
    int *sock;

    dp_connp dpc = dpinit();
    if (dpc == NULL) {
        perror("drexel protocol create failure"); 
        return NULL;
    }

    sock = &(dpc->udp_sock);
    servaddr = &(dpc->outSockAddr.addr);

    // Creating socket file descriptor 
    if ( (*sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        return NULL;
    } 

    // Filling server information 
    servaddr->sin_family = AF_INET; 
    servaddr->sin_port = htons(port); 
    servaddr->sin_addr.s_addr = inet_addr(addr);
    dpc->outSockAddr.len = sizeof(struct sockaddr_in); 
    dpc->outSockAddr.isAddrInit = true;

    // The inbound address is the same as the outbound address
    memcpy(&dpc->inSockAddr, &dpc->outSockAddr, sizeof(dpc->outSockAddr));

    return dpc;
}

/*
* 
*/
int dprecv(dp_connp dp, void *buff, int buff_sz){

    dp_pdu *inPdu;
    int rcvLen = dprecvdgram(dp, _dpBuffer, sizeof(_dpBuffer));

    if(rcvLen == DP_CONNECTION_CLOSED)
        return DP_CONNECTION_CLOSED;

    inPdu = (dp_pdu *)_dpBuffer;
    if(rcvLen > sizeof(dp_pdu))
        memcpy(buff, (_dpBuffer+sizeof(dp_pdu)), inPdu->dgram_sz);

    return inPdu->dgram_sz;
}


static int dprecvdgram(dp_connp dp, void *buff, int buff_sz){
    int bytesIn = 0;
    int errCode = DP_NO_ERROR;

    if(buff_sz > DP_MAX_DGRAM_SZ)
        return DP_BUFF_OVERSIZED;

    bytesIn = dprecvraw(dp, buff, buff_sz);

    //check for some sort of error and just return it
    if (bytesIn < sizeof(dp_pdu))
        errCode = DP_ERROR_BAD_DGRAM;

    dp_pdu inPdu;
    memcpy(&inPdu, buff, sizeof(dp_pdu));
    if (inPdu.dgram_sz > buff_sz)
        errCode = DP_BUFF_UNDERSIZED;

    //Copy buffer back
    // memcpy(buff, (_dpBuffer+sizeof(dp_pdu)), inPdu.dgram_sz);
    
    
    //UDPATE SEQ NUMBER AND PREPARE ACK
    if (errCode == DP_NO_ERROR){
        if(inPdu.dgram_sz == 0)
            //Update Seq Number to just ack a control message - just got PDU
            dp->seqNum ++;
        else
            //Update Seq Number to increas by the inbound PDU dgram_sz
            dp->seqNum += inPdu.dgram_sz;
    } else {
        //Update seq number to ACK error
        dp->seqNum++;
    }

    dp_pdu outPdu;
    outPdu.proto_ver = DP_PROTO_VER_1;
    outPdu.dgram_sz = 0;
    outPdu.seqnum = dp->seqNum;
    outPdu.err_num = errCode;

    int actSndSz = 0;
    //HANDLE ERROR SITUATION
    if(errCode != DP_NO_ERROR) {
        outPdu.mtype = DP_MT_ERROR;
        actSndSz = dpsendraw(dp, &outPdu, sizeof(dp_pdu));
        if (actSndSz != sizeof(dp_pdu))
            return DP_ERROR_PROTOCOL;
    }


    switch(inPdu.mtype){
        case DP_MT_SND:
            outPdu.mtype = DP_MT_SNDACK;
            actSndSz = dpsendraw(dp, &outPdu, sizeof(dp_pdu));
            if (actSndSz != sizeof(dp_pdu))
                return DP_ERROR_PROTOCOL;
            break;
        case DP_MT_CLOSE:
            outPdu.mtype = DP_MT_CLOSEACK;
            actSndSz = dpsendraw(dp, &outPdu, sizeof(dp_pdu));
            if (actSndSz != sizeof(dp_pdu))
                return DP_ERROR_PROTOCOL;
            dpclose(dp);
            return DP_CONNECTION_CLOSED;
        default:
        {
            printf("ERROR: Unexpected or bad mtype in header %d\n", inPdu.mtype);
            return DP_ERROR_PROTOCOL;
        }
    }

    return bytesIn;
}

/*
* static int dprecvraw(dp_connp dp, void *buff, int buff_sz) 
*/
static int dprecvraw(dp_connp dp, void *buff, int buff_sz){
    int bytes = 0;

    if(!dp->inSockAddr.isAddrInit) {
        perror("dprecv: dp connection not setup properly - cli struct not init");
        return -1;
    }

    bytes = recvfrom(dp->udp_sock, (char *)buff, buff_sz,  
                MSG_WAITALL, ( struct sockaddr *) &(dp->outSockAddr.addr), 
                &(dp->outSockAddr.len)); 

    if (bytes < 0) {
        perror("dprecv: received error from recvfrom()");
        return -1;
    }
    dp->outSockAddr.isAddrInit = true;

    //some helper code if you want to do debugging
    if (bytes > sizeof(dp_pdu)){
        if(false) {                         //just diabling for now
            dp_pdu *inPdu = buff;
            char * payload = (char *)buff + sizeof(dp_pdu);
            printf("DATA : %.*s\n", inPdu->dgram_sz , payload); 
        }
    }

    dp_pdu *inPdu = buff;
    print_in_pdu(inPdu);

    //return the number of bytes received 
    return bytes;
}

int dpsend(dp_connp dp, void *sbuff, int sbuff_sz){


    //For now, we will not be able to send larger than the biggest datagram
    if(sbuff_sz > dpmaxdgram()) {
        return DP_BUFF_UNDERSIZED;
    }

    int sndSz = dpsenddgram(dp, sbuff, sbuff_sz);

    return sndSz;
}

static int dpsenddgram(dp_connp dp, void *sbuff, int sbuff_sz){
    int bytesOut = 0;

    if(!dp->outSockAddr.isAddrInit) {
        perror("dpsend:dp connection not setup properly");
        return DP_ERROR_GENERAL;
    }

    if(sbuff_sz > DP_MAX_BUFF_SZ)
        return DP_ERROR_GENERAL;

    //Build the PDU and out buffer
    dp_pdu *outPdu = (dp_pdu *)_dpBuffer;
    int    sndSz = sbuff_sz;
    outPdu->proto_ver = DP_PROTO_VER_1;
    outPdu->mtype = DP_MT_SND;
    outPdu->dgram_sz = sndSz;
    outPdu->seqnum = dp->seqNum;

    memcpy((_dpBuffer + sizeof(dp_pdu)), sbuff, sndSz);

    int totalSendSz = outPdu->dgram_sz + sizeof(dp_pdu);
    bytesOut = dpsendraw(dp, _dpBuffer, totalSendSz);

    if(bytesOut != totalSendSz){
        printf("Warning send %d, but expected %d!\n", bytesOut, totalSendSz);
    }

    //update seq number after send
    if(outPdu->dgram_sz == 0)
        dp->seqNum++;
    else
        dp->seqNum += outPdu->dgram_sz;

    //need to get an ack
    dp_pdu inPdu = {0};
    int bytesIn = dprecvraw(dp, &inPdu, sizeof(dp_pdu));
    if ((bytesIn < sizeof(dp_pdu)) && (inPdu.mtype != DP_MT_SNDACK)){
        printf("Expected SND/ACK but got a different mtype %d\n", inPdu.mtype);
    }

    return bytesOut - sizeof(dp_pdu);
}


static int dpsendraw(dp_connp dp, void *sbuff, int sbuff_sz){
    int bytesOut = 0;
    // dp_pdu *pdu = sbuff;

    if(!dp->outSockAddr.isAddrInit) {
        perror("dpsendraw:dp connection not setup properly");
        return -1;
    }

    dp_pdu *outPdu = sbuff;
    bytesOut = sendto(dp->udp_sock, (const char *)sbuff, sbuff_sz, 
        0, (const struct sockaddr *) &(dp->outSockAddr.addr), 
            dp->outSockAddr.len); 

    
    print_out_pdu(outPdu);

    return bytesOut;
}


int dplisten(dp_connp dp) {
    int sndSz, rcvSz;

    if(!dp->inSockAddr.isAddrInit) {
        perror("dplisten:dp connection not setup properly - cli struct not init");
        return DP_ERROR_GENERAL;
    }

    dp_pdu pdu = {0};

    printf("Waiting for a connection...\n");
    rcvSz = dprecvraw(dp, &pdu, sizeof(pdu));
    if (rcvSz != sizeof(pdu)) {
        perror("dplisten:The wrong number of bytes were received");
        return DP_ERROR_GENERAL;
    }

    pdu.mtype = DP_MT_CNTACK;
    dp->seqNum = pdu.seqnum + 1;
    pdu.seqnum = dp->seqNum;
    
    sndSz = dpsendraw(dp, &pdu, sizeof(pdu));
    
    if (sndSz != sizeof(pdu)) {
        perror("dplisten:The wrong number of bytes were sent");
        return DP_ERROR_GENERAL;
    }
    dp->isConnected = true; 
    //For non data transmissions, ACK of just control data increase seq # by one
    printf("Connection established OK!\n");

    return true;
}

int dpconnect(dp_connp dp) {

    int sndSz, rcvSz;

    if(!dp->outSockAddr.isAddrInit) {
        perror("dpconnect:dp connection not setup properly - svr struct not init");
        return DP_ERROR_GENERAL;
    }

    dp_pdu pdu = {0};
    pdu.mtype = DP_MT_CONNECT;
    pdu.seqnum = dp->seqNum;
    pdu.dgram_sz = 0;

    sndSz = dpsendraw(dp, &pdu, sizeof(pdu));
    if (sndSz != sizeof(dp_pdu)) {
        perror("dpconnect:Wrong about of connection data sent");
        return -1;
    }
    
    rcvSz = dprecvraw(dp, &pdu, sizeof(pdu));
    if (rcvSz != sizeof(dp_pdu)) {
        perror("dpconnect:Wrong about of connection data received");
        return -1;
    }
    if (pdu.mtype != DP_MT_CNTACK) {
        perror("dpconnect:Expected CNTACT Message but didnt get it");
        return -1;
    }

    //For non data transmissions, ACK of just control data increase seq # by one
    dp->seqNum++;
    dp->isConnected = true;
    printf("Connection established OK!\n");

    return true;
}

int dpdisconnect(dp_connp dp) {

    int sndSz, rcvSz;

    dp_pdu pdu = {0};
    pdu.proto_ver = DP_PROTO_VER_1;
    pdu.mtype = DP_MT_CLOSE;
    pdu.seqnum = dp->seqNum;
    pdu.dgram_sz = 0;

    sndSz = dpsendraw(dp, &pdu, sizeof(pdu));
    if (sndSz != sizeof(dp_pdu)) {
        perror("dpdisconnect:Wrong about of connection data sent");
        return DP_ERROR_GENERAL;
    }
    
    rcvSz = dprecvraw(dp, &pdu, sizeof(pdu));
    if (rcvSz != sizeof(dp_pdu)) {
        perror("dpdisconnect:Wrong about of connection data received");
        return DP_ERROR_GENERAL;
    }
    if (pdu.mtype != DP_MT_CLOSEACK) {
        perror("dpdisconnect:Expected CNTACT Message but didnt get it"); 
        return DP_ERROR_GENERAL;
    }
    //For non data transmissions, ACK of just control data increase seq # by one
    dpclose(dp);

    return DP_CONNECTION_CLOSED;
}

void * dp_prepare_send(dp_pdu *pdu_ptr, void *buff, int buff_sz) {
    if (buff_sz < sizeof(dp_pdu)) {
        perror("Expected CNTACT Message but didnt get it");
        return NULL;
    }
    bzero(buff, buff_sz);
    memcpy(buff, pdu_ptr, sizeof(dp_pdu));

    return buff + sizeof(dp_pdu);
}


//// MISC HELPERS

/*
* void print_out_pdu(dp_pdu *pdu) uses a pointer to our populated pdu and checks to see if we have debug mode on. If we do not have it on, we return right away.
* Otherwise we print our header to signify we are SENDING a pdu and then call print_pdu_details to print the fields.
*/
void print_out_pdu(dp_pdu *pdu) {
    if (_debugMode != 1)
        return;
    printf("PDU DETAILS ===>  [OUT]\n");
    print_pdu_details(pdu);
}

/*
* void print_in_pdu(dp_pdu *pdu) uses a pointer to our populated pdu and checks to see if we have debug mode on. If we do not have it on, we return right away.
* Otherwise we print our header to signify we are RECEIVING a pdu and then call print_pdu_details to print the fields.
*/
void print_in_pdu(dp_pdu *pdu) {
    if (_debugMode != 1)
        return;
    printf("===> PDU DETAILS  [IN]\n");
    print_pdu_details(pdu);
}

/*
* static void print_pdu_details(dp_pdu *pdu) takes a pointer to a populated pdu and simply prints out the fields with proper headings or labels.
* This is so we can see what each pdu contains since the details remain the same whether the pdu is being received or sent out. print_pdu_details()
* uses pdu_msg_to_string to print the 'Msg Type' field.
*/
static void print_pdu_details(dp_pdu *pdu){
    
    printf("\tVersion:  %d\n", pdu->proto_ver);
    printf("\tMsg Type: %s\n", pdu_msg_to_string(pdu));
    printf("\tMsg Size: %d\n", pdu->dgram_sz);
    printf("\tSeq Numb: %d\n", pdu->seqnum);
    printf("\n");
}

/*
* static char * pdu_msg_to_string(dp_pdu *pdu) simply takes in a pointer to our populated pdu and looks at the field
* called 'mtype' which is short for message type. These are growing powers of two so that each message type has exactly
* one bit on with the rest off in the byte that makes up mtype. We feed it into a switch statement using defined 'magic-
* numbers' in our du-proto.h file and then return string of what the message really is in human-readable english.
*/
static char * pdu_msg_to_string(dp_pdu *pdu) {
    switch(pdu->mtype){
        case DP_MT_ACK:
            return "ACK";     
        case DP_MT_SND:
            return "SEND";      
        case DP_MT_CONNECT:
            return "CONNECT";   
        case DP_MT_CLOSE:
            return "CLOSE";     
        case DP_MT_NACK:
            return "NACK";      
        case DP_MT_SNDACK:
            return "SEND/ACK";    
        case DP_MT_CNTACK:
            return "CONNECT/ACK";    
        case DP_MT_CLOSEACK:
            return "CLOSE/ACK";
        default:
            return "***UNKNOWN***";  
    }
}

/*
 *  This is a helper for testing if you want to inject random errors from
 *  time to time. It take a threshold number as a paramter and behaves as
 *  follows:
 *      if threshold is < 1 it always returns FALSE or zero
 *      if threshold is > 99 it always returns TRUE or 1
 *      if (1 <= threshold <= 99) it generates a random number between
 *          1..100 and if the random number is less than the threshold
 *          it returns TRUE, else it returns false
 * 
 *  Example: dprand(50) is a coin flip
 *              dprand(25) will return true 25% of the time
 *              dprand(99) will return true 99% of the time
 */
int dprand(int threshold){

    if (threshold < 1)
        return 0;
    if (threshold > 99)
        return 1;
    //initialize randome number seed
    srand(time(0));

    int rndInRange = (rand() % (100-1+1)) + 1;
    if (threshold < rndInRange)
        return 1;
    else
        return 0;
}
