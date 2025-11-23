// ftp-debug.c (or du-ftp.c bottom)

#include <stdio.h>
#include <string.h>
#include "du-ftp.h"

static int _ftpDebugMode = 1;

static const char * ftp_msg_to_string(const ftp_pdu *pdu) {
    switch (pdu->msg_type) {
        case MSG_FILE_REQUEST: return "FILE_REQUEST";
        case MSG_FILE_OK:      return "FILE_OK";
        case MSG_FILE_ERR:     return "FILE_ERR";
        case MSG_DATA:         return "DATA";
        case MSG_DATA_OK:      return "DATA_OK";
        case MSG_DATA_END:     return "DATA_END";
        case MSG_CLOSE:        return "CLOSE";
        case MSG_ERROR:        return "ERROR";
        default:               return "***UNKNOWN***";
    }
}

static void print_ftp_pdu_details(const ftp_pdu *pdu) {

    printf("\tMsg Type:     %s (%d)\n", ftp_msg_to_string(pdu), pdu->msg_type);
    printf("\tFile Size:    %ld\n", (long)pdu->file_size);
    printf("\tFile Name:    %.*s\n", (int)sizeof(pdu->file_name), pdu->file_name);
    printf("\tByte Number:  %d\n", pdu->byte_number);
    printf("\tPayload Size: %d\n", pdu->payload_size);
    printf("\n");
}

void print_out_ftp_pdu(const ftp_pdu *pdu) {
    if (_ftpDebugMode != 1) return;
    printf("FTP PDU DETAILS ===> [OUT]\n");
    print_ftp_pdu_details(pdu);
}

void print_in_ftp_pdu(const ftp_pdu *pdu) {
    if (_ftpDebugMode != 1) return;
    printf("FTP PDU DETAILS ===> [IN]\n");
    print_ftp_pdu_details(pdu);
}

void ftp_set_debug(int enabled) {
    _ftpDebugMode = enabled ? 1 : 0;
}
