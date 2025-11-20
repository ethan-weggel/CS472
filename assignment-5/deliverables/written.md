**Ethan Weggel**

**erw74**
### PART ONE
---

/*
* dp_connp_dpinit() is a static function that exists only in the context of this file (du-proto.c). 
* The goal of the function is to create a new instance of a dp_connection struct and initialize the values
* from random memory to useful starting values or zeroes. We start by declaring a variable 'dpsession' that is of
* type dp_connp which is typdef'd to represent a pointer to a dp_connection. This memory is pulled from the heap
* (however many bytes one dp_connection struct takes, denoted by sizseof(dp_connection)) and we save the pointer to this
* chunk of memory. We then initialize all bytes to the value of zero using bzero() and passing our pointer. Then for
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

`static dp_connp dpinit()`


/*
* dp_close(dp_connp dpsession) simply takes an instance of dp_connp which is a pointer to a struct 'dp_connection'.
* This function then takes that pointer and frees the memory used to hold all fields and returns the memory to the heap
* so there are no memory leaks or resource problems in the program.
*/

`void dpclose(dp_connp dpsession)`


/*
* dpmaxdgram() simply returns a constant DP_MAX_BUFF_SZ. This is useful because DP_MAX_BUFF_SZ is used in other places
* to allocate memory to hold a datagram, but really, this also represents the maximum datagram we can store. It's a handy
* way to use one constant or 'magic number' and give it a second name within our program's context.
*/

`int  dpmaxdgram()`


/*
* dpServerInit(int port) at its core takes a port number and returns a server socket that is receptive to clients connecting
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

`dp_connp dpServerInit(int port)`


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

`dp_connp dpClientInit(char *addr, int port)`


`int dprecv(dp_connp dp, void *buff, int buff_sz)`


`static int dprecvdgram(dp_connp dp, void *buff, int buff_sz)`


`static int dprecvraw(dp_connp dp, void *buff, int buff_sz)`


`int dpsend(dp_connp dp, void *sbuff, int sbuff_sz)`


`static int dpsenddgram(dp_connp dp, void *sbuff, int sbuff_sz)`


`static int dpsendraw(dp_connp dp, void *sbuff, int sbuff_sz)`


`int dplisten(dp_connp dp)`


`int dpconnect(dp_connp dp)`


`int dpdisconnect(dp_connp dp)`


`void * dp_prepare_send(dp_pdu *pdu_ptr, void *buff, int buff_sz)`


/*
* void print_out_pdu(dp_pdu *pdu) uses a pointer to our populated pdu and checks to see if we have debug mode on. If we do not have it on, we return right away.
* Otherwise we print our header to signify we are SENDING a pdu and then call print_pdu_details to print the fields.
*/

`void print_out_pdu(dp_pdu *pdu)`


/*
* void print_in_pdu(dp_pdu *pdu) uses a pointer to our populated pdu and checks to see if we have debug mode on. If we do not have it on, we return right away.
* Otherwise we print our header to signify we are RECEIVING a pdu and then call print_pdu_details to print the fields.
*/

`void print_in_pdu(dp_pdu *pdu)`


/*
* static void print_pdu_details(dp_pdu *pdu) takes a pointer to a populated pdu and simply prints out the fields with proper headings or labels.
* This is so we can see what each pdu contains since the details remain the same whether the pdu is being received or sent out. print_pdu_details()
* uses pdu_msg_to_string to print the 'Msg Type' field.
*/

`static void print_pdu_details(dp_pdu *pdu)`


/*
* static char * pdu_msg_to_string(dp_pdu *pdu) simply takes in a pointer to our populated pdu and looks at the field
* called 'mtype' which is short for message type. These are growing powers of two so that each message type has exactly
* one bit on with the rest off in the byte that makes up mtype. We feed it into a switch statement using defined 'magic-
* numbers' in our du-proto.h file and then return string of what the message really is in human-readable english.
*/

`static char * pdu_msg_to_string(dp_pdu *pdu)`


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

`int dprand(int threshold)`

---
### PART TWO
---