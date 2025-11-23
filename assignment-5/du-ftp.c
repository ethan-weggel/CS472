#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include "du-ftp.h"
#include "du-proto.h"
#include "utilities.h"
#include "ftp-debug.h"

#define BUFF_SZ (2 * DP_MAX_DGRAM_SZ)
static char sbuffer[BUFF_SZ];
static char rbuffer[BUFF_SZ];
static char full_file_path[FNAME_SZ];

/*
 *  Helper function that processes the command line arguements.  Highlights
 *  how to use a very useful utility called getopt, where you pass it a
 *  format string and it does all of the hard work for you.  The arg
 *  string basically states this program accepts a -p or -c flag, the
 *  -p flag is for a "pong message", in other words the server echos
 *  back what the client sends, and a -c message, the -c option takes
 *  a course id, and the server looks up the course id and responds
 *  with an appropriate message. 
 */
static int initParams(int argc, char *argv[], prog_config *cfg) {
    int option;
    //setup defaults if no arguements are passed
    static char cmdBuffer[64] = {0};

    //setup defaults if no arguements are passed
    cfg->prog_mode = PROG_MD_CLI;
    cfg->port_number = DEF_PORT_NO;
    strcpy(cfg->file_name, PROG_DEF_FNAME);
    strcpy(cfg->svr_ip_addr, PROG_DEF_SVR_ADDR);
    
    while ((option = getopt(argc, argv, ":p:f:a:csh")) != -1) {
        switch(option) {
            case 'p':
                strncpy(cmdBuffer, optarg, sizeof(cmdBuffer));
                cfg->port_number = atoi(cmdBuffer);
                break;
            case 'f':
                strncpy(cfg->file_name, optarg, sizeof(cfg->file_name));
                break;
            case 'a':
                strncpy(cfg->svr_ip_addr, optarg, sizeof(cfg->svr_ip_addr));
                break;
            case 'c':
                cfg->prog_mode = PROG_MD_CLI;
                break;
            case 's':
                cfg->prog_mode = PROG_MD_SVR;
                break;
            case 'h':
                printf("USAGE: %s [-p port] [-f fname] [-a svr_addr] [-s] [-c] [-h]\n", argv[0]);
                printf("WHERE:\n\t[-c] runs in client mode, [-s] runs in server mode; DEFAULT= client_mode\n");
                printf("\t[-a svr_addr] specifies the servers IP address as a string; DEFAULT = %s\n", cfg->svr_ip_addr);
                printf("\t[-p portnum] specifies the port number; DEFAULT = %d\n", cfg->port_number);
                printf("\t[-f fname] specifies the filename to send or recv; DEFAULT = %s\n", cfg->file_name);
                printf("\t[-p] displays what you are looking at now - the help\n\n");
                exit(0);
            case ':':
                perror ("Option missing value");
                exit(-1);
            default:
            case '?':
                perror ("Unknown option");
                exit(-1);
        }
    }
    return cfg->prog_mode;
}

int server_loop(dp_connp dpc, void *sBuff, void *rBuff, int sbuff_sz, int rbuff_sz) {
    int rcvSz;
    ftp_pdu* recvPdu;
    ftp_pdu sendPdu;
    FILE* f = NULL;

    if (dpc->isConnected == false) {
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }

    // Loop until a disconnect is received, or error happens
    while (1) {
        memset(&sendPdu, 0, sizeof(ftp_pdu));

        // receive request from client
        rcvSz = dprecv(dpc, rBuff, rbuff_sz);
        if (rcvSz == DP_CONNECTION_CLOSED){
            if (f != NULL) {
                fclose(f);
            }
            printf("Client closed connection\n");
            return DP_CONNECTION_CLOSED;
        }

        // get our pdu
        recvPdu = (ftp_pdu*) rBuff;
        print_in_ftp_pdu(recvPdu);

        // event handling
        switch (recvPdu->msg_type) {
            case MSG_FILE_REQUEST:
                printf("Received request to start new transfer!\n");
                char tmp[128] = {"./infile/"};
                f = fopen(strcat(tmp, recvPdu->file_name), "wb+");
                if (f == NULL) {
                    printf("ERROR:  Cannot open file %s\n", full_file_path);
                    sendPdu.msg_type = MSG_FILE_ERR;
                } else {
                    sendPdu.msg_type = MSG_FILE_OK;
                }

                sendPdu.file_size = 0;
                memcpy(&sendPdu.file_name, recvPdu->file_name, sizeof(recvPdu->file_name));
                sendPdu.byte_number = 0;
                sendPdu.payload_size = 0;
                break;
            case MSG_DATA:
                char* payload = rBuff + sizeof(ftp_pdu);
                int payload_size = recvPdu->payload_size;
                int bytesWritten = fwrite(payload, 1, payload_size, f);

                if (bytesWritten != recvPdu->payload_size) {
                    sendPdu.msg_type = MSG_ERROR;
                } else {
                    sendPdu.msg_type = MSG_DATA_OK;
                }

                int toPrint = payload_size > 50 ? 50 : payload_size;

                printf("========================> \n%.*s\n========================> \n", toPrint, payload);

                sendPdu.file_size = recvPdu->file_size;
                memcpy(&sendPdu.file_name, recvPdu->file_name, sizeof(recvPdu->file_name));
                sendPdu.byte_number = recvPdu->byte_number;
                sendPdu.payload_size = 0;
                break;
            case MSG_DATA_END:
                printf("Client ended transfer! Quitting...\n");

                sendPdu.msg_type = MSG_CLOSE;
                sendPdu.file_size = recvPdu->file_size;
                memcpy(&sendPdu.file_name, recvPdu->file_name, sizeof(recvPdu->file_name));
                sendPdu.byte_number = recvPdu->byte_number;
                sendPdu.payload_size = 0;

                memcpy(sBuff, &sendPdu, sizeof(ftp_pdu));
                print_out_ftp_pdu(&sendPdu);
                dpsend(dpc, sBuff, sizeof(ftp_pdu));

                if (f != NULL) {
                    fclose(f);
                }

                return DP_CONNECTION_CLOSED;
            default:
                printf("Received unknown message type. Ignoring...\n");
                break;
        }

        // send pdu back to client
        memcpy(sBuff, &sendPdu, sizeof(ftp_pdu));
        print_out_ftp_pdu(&sendPdu);
        dpsend(dpc, sBuff, sizeof(ftp_pdu));
        if (sendPdu.msg_type == MSG_ERROR) {
            exit(-1);
        }
    }

}


void start_client(dp_connp dpc, prog_config* cfg) {
    static char sBuff[2 * DP_MAX_BUFF_SZ];


    if (!dpc->isConnected) {
        printf("Client not connected\n");
        return;
    }

    // Start of du-ftp handshake

    // populate our pdu
    ftp_pdu pdu;
    long fileSz = get_file_size(full_file_path);
    if (fileSz < 0) {
        perror("get_file_size: general error getting file size");
        return;
    }

    pdu.msg_type = MSG_FILE_REQUEST;
    pdu.file_size = fileSz;
    memcpy(&pdu.file_name, cfg->file_name, sizeof(cfg->file_name));
    pdu.byte_number = 0;
    pdu.payload_size = 0;

    // send and receive back from server

    // copy pdu into send buffer
    memcpy(sBuff, &pdu, sizeof(ftp_pdu));
    print_out_ftp_pdu(&pdu);
    dpsend(dpc, sBuff, sizeof(ftp_pdu));


    // receive server response pdu
    int bytesRecv = dprecv(dpc, rbuffer, sizeof(rbuffer));
    ftp_pdu* recvPdu = (ftp_pdu*) rbuffer;
    print_in_ftp_pdu(recvPdu);

    // see server's response
    if (recvPdu->msg_type == MSG_FILE_ERR) {
        printf("Error setting up file on server side. Quitting...\n");
        dpdisconnect(dpc);
        exit(-1);
    } else {
        printf("Server ready to receive file data!\n");
    }

    // we are ready to send file data in chunks; open file
    FILE *f = fopen(full_file_path, "rb");
    if (f == NULL) {
        printf("ERROR:  Cannot open file %s\n", full_file_path);
        exit(-1);
    }
    if (dpc->isConnected == false) {
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }

    int bytes = 0;
    int byte_number = 0;
    // copy file bytes into send array, leaving gap for pdu to fill in later
    while ((bytes = fread(sBuff+sizeof(ftp_pdu), 1, sizeof(sBuff)-sizeof(ftp_pdu), f )) > 0) {

        // set up new pdu
        memset(&pdu, 0, sizeof(ftp_pdu));
        pdu.msg_type = MSG_DATA;
        pdu.file_size = fileSz;
        memcpy(&pdu.file_name, cfg->file_name, sizeof(cfg->file_name));
        pdu.byte_number = byte_number;
        pdu.payload_size = bytes;
        byte_number += bytes; // increment our byte number from our current number to what was sent
        
        // fill in pdu to sBuff
        memcpy(sBuff, &pdu, sizeof(ftp_pdu));
        print_out_ftp_pdu(&pdu);
        // send that thang yo
        dpsend(dpc, sBuff, sizeof(ftp_pdu)+bytes);

        // check for writing error on server side
        memset(rbuffer, 0, sizeof(rbuffer));
        bytesRecv = dprecv(dpc, rbuffer, sizeof(rbuffer));
        if (bytesRecv == DP_CONNECTION_CLOSED) {
            printf("Server disconnected early!\n");
            return;
        }

        ftp_pdu* recvPdu = (ftp_pdu*) rbuffer;
        print_in_ftp_pdu(recvPdu);
        if (recvPdu->msg_type == MSG_ERROR) {
            printf("Server had error writing file. Quitting...\n");
            exit(-1);
        }

    }

    // set up final close-pdu
    memset(&pdu, 0, sizeof(ftp_pdu));
    pdu.msg_type = MSG_DATA_END;
    pdu.file_size = fileSz;
    memcpy(&pdu.file_name, cfg->file_name, sizeof(cfg->file_name));
    pdu.byte_number = byte_number;
    pdu.payload_size = 0;

    // copy pdu into send buffer again
    memcpy(sBuff, &pdu, sizeof(ftp_pdu));
    print_out_ftp_pdu(&pdu);
    // send pdu
    dpsend(dpc, sBuff, sizeof(ftp_pdu));

    // receive server response
    bytesRecv = dprecv(dpc, rbuffer, sizeof(rbuffer));
    recvPdu = (ftp_pdu*) rbuffer;
    print_in_ftp_pdu(recvPdu);

    // event handling
    switch (recvPdu->msg_type) {
        case MSG_ERROR:
            printf("Server responded with error trying to end transfer. Quitting...\n");
            break;
        case MSG_CLOSE:
            printf("Server successfully ended transfer! Quitting...\n");
            break;
        default:
            printf("Unknown response. Quitting...\n");
            break;
    }
    fclose(f);
    // dpdisconnect(dpc);
    return;
}

void start_server(dp_connp dpc){
    server_loop(dpc, sbuffer, rbuffer, sizeof(sbuffer), sizeof(rbuffer));
}


int main(int argc, char *argv[]) {
    prog_config cfg;
    int cmd;
    dp_connp dpc;
    int rc;


    //Process the parameters and init the header - look at the helpers
    //in the cs472-pproto.c file
    cmd = initParams(argc, argv, &cfg);

    printf("MODE %d\n", cfg.prog_mode);
    printf("PORT %d\n", cfg.port_number);
    printf("FILE NAME: %s\n", cfg.file_name);

    switch (cmd) {
        case PROG_MD_CLI:
            //by default client will look for files in the ./outfile directory
            snprintf(full_file_path, sizeof(full_file_path), "./outfile/%s", cfg.file_name);
            dpc = dpClientInit(cfg.svr_ip_addr,cfg.port_number);
            rc = dpconnect(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_client(dpc, &cfg);
            exit(0);
            break;

        case PROG_MD_SVR:
            dpc = dpServerInit(cfg.port_number);
            rc = dplisten(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_server(dpc);
            break;
        default:
            printf("ERROR: Unknown Program Mode.  Mode set is %d\n", cmd);
            break;
    }
}