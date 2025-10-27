#ifndef __CRYPTO_SERVER_H__
#define __CRYPTO_SERVER_H__

#include "protocol.h"
#include "crypto-lib.h"
#include <stdio.h>

#define BACKLOG 5
#define RC_CLIENT_EXITED            1
#define RC_CLIENT_REQ_SERVER_EXIT   2

void start_server(const char* addr, int port);
int server_loop(int server_socket, const char* addr, int port);
int service_client_loop(int client_socket);

#endif // __CRYPTO_SERVER_H__