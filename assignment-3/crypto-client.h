#ifndef __CRYPTO_CLIENT_H__
#define __CRYPTO_CLIENT_H__

#include "protocol.h"
#include "crypto-lib.h"
#include <stdio.h>

typedef struct msg_cmd {
    int cmd_id; 
    char *cmd_line; 
} msg_cmd_t;


#define CMD_EXECUTE 0 
#define CMD_NO_EXEC 1 

void start_client(const char* addr, int port);
int get_command(char *cmd_buff, size_t cmd_buff_sz, msg_cmd_t *msg_cmd);
int client_loop(int sockfd);

#endif // __CRYPTO_CLIENT_H__