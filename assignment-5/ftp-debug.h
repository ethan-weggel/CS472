#ifndef __FTP_DEBUG_H__
#define __FTP_DEBUG_H__

#include "du-ftp.h"

void print_out_ftp_pdu(const ftp_pdu *pdu);
void print_in_ftp_pdu(const ftp_pdu *pdu);
void ftp_set_debug(int enabled); 

#endif