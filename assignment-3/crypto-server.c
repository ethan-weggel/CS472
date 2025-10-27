#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include "crypto-server.h"
#include "crypto-lib.h"
#include "protocol.h"
#include <errno.h>
#include <inttypes.h>

static int extract_crypto_msg_data(const uint8_t* pdu_buff, uint16_t pdu_len, char* msg_str, uint16_t max_str_len, uint8_t* out_type, uint8_t* out_dir);
static uint8_t compute_hash(const char *message, size_t len);
static ssize_t send_all(int sockfd, const char* buffer, size_t length);


static int make_echo(char *dst, size_t dst_sz, const char *src);
void start_server(const char* addr, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (strcmp("0.0.0.0", addr) == 0) {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, addr, &server_addr.sin_addr);
    }

    bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(sockfd, BACKLOG);

    int rc = server_loop(sockfd, addr, port);
    printf("Server exited with return code %d\n", rc);

    close(sockfd);
    return;
}

int server_loop(int server_socket, const char* addr, int port) {
    printf("Server listening on %s:%d...\n", addr, port);

    int shutdown_requested = 0;
    int rc;
    while (!shutdown_requested) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        printf("Client connected...\n");
        rc = service_client_loop(client_sock);
        if (rc == 10) {
            shutdown_requested = 1;
            rc = 0;
        }
        close(client_sock);
    }
    return rc;
}

int service_client_loop(int client_socket) {
    char pdu_buff[BUFFER_SIZE];
    crypto_key_t server_key, client_key;
    
    while (1) {
        ssize_t bytes_read = recv(client_socket, pdu_buff, sizeof(pdu_buff), 0);

        if (bytes_read > 0) {

            char msg_str[MAX_MSG_DATA_SIZE];

            int pdu_length_validation = extract_crypto_msg_data((uint8_t*)pdu_buff, (uint16_t)bytes_read, msg_str, MAX_MSG_DATA_SIZE, NULL, NULL);
            if (pdu_length_validation != 0) {
                printf("Recieved PDU length does not equal header size plus payload size...\n");
                continue;
            }

            crypto_msg_t *pdu = (crypto_msg_t *)pdu_buff;
            

            switch (pdu->header.msg_type) {
                case MSG_KEY_EXCHANGE:
                    pdu->header.msg_type = MSG_KEY_EXCHANGE;
                    pdu->header.direction = DIR_RESPONSE;
                    pdu->header.payload_len = htons((uint16_t)sizeof(crypto_key_t));
                    if (gen_key_pair(&server_key, &client_key) == RC_OK) {
                        memcpy(pdu->payload, &client_key, sizeof(crypto_key_t));
                    } else {
                        char err_str[] = "Error generating key exchange...";
                        memcpy(pdu->payload, err_str, sizeof(err_str));
                        send_all(client_socket, pdu_buff, sizeof(crypto_pdu_t) + sizeof(err_str));
                        continue;
                    }
                    send_all(client_socket, pdu_buff, sizeof(crypto_pdu_t) + sizeof(crypto_key_t));
                    continue;
                case MSG_ENCRYPTED_DATA: {
                    uint8_t decrypted[MAX_MSG_DATA_SIZE];
                    uint8_t encrypted[MAX_MSG_DATA_SIZE];
                    uint16_t in_len = ntohs(pdu->header.payload_len);  

                    int decrypted_length = decrypt_string(server_key, decrypted, pdu->payload, in_len);
                    if (decrypted_length > 0) {
                        decrypted[decrypted_length] = '\0';
                    }

                    char echo_msg[MAX_MSG_DATA_SIZE];
                    int echo_ret_len = make_echo(echo_msg, sizeof(echo_msg), (const char *)decrypted);

                    int encrypted_length = encrypt_string(server_key, encrypted, (uint8_t*)echo_msg, (size_t)echo_ret_len);
                    if (encrypted_length > 0) {
                        memcpy(pdu->payload, encrypted, encrypted_length);
                        pdu->header.payload_len = encrypted_length;
                    }

                    pdu->header.msg_type = MSG_ENCRYPTED_DATA;
                    pdu->header.direction = DIR_RESPONSE;
                    pdu->header.payload_len = htons((uint16_t)encrypted_length);
                    size_t total_pdu_echo_len = sizeof(crypto_pdu_t) + (size_t)encrypted_length;

                    ssize_t bytes_sent = send_all(client_socket, pdu_buff, total_pdu_echo_len);
                    (void)bytes_sent;
                    continue;
                }
                case MSG_DIG_SIGNATURE: {
                    uint8_t decrypted[MAX_MSG_DATA_SIZE];
                    uint8_t encrypted_sig;
                    uint8_t computed_hash;
                    uint8_t received_hash;
                    uint8_t encrypted[MAX_MSG_DATA_SIZE];
                    uint16_t in_len = ntohs(pdu->header.payload_len);  

                    encrypted_sig = pdu->payload[0];
                    decrypt(server_key, &received_hash, &encrypted_sig, 1);

                    int decrypted_length = decrypt_string(server_key, decrypted, pdu->payload+1, in_len-1);
                    if (decrypted_length > 0) {
                        decrypted[decrypted_length] = '\0';
                    }

                    computed_hash = compute_hash((const char *)decrypted, (size_t)decrypted_length);
                    computed_hash &= 0x3F;

                    if (received_hash != computed_hash) {
                        printf("[ERROR] Digital signature verification failed!\n");
                        pdu->header.msg_type = MSG_ERROR;
                        pdu->header.direction = DIR_RESPONSE;
                        memset(pdu->payload, 0, in_len);  
                        send_all(client_socket, pdu_buff, sizeof(crypto_msg_t) + in_len);
                    }

                    char echo_msg[MAX_MSG_DATA_SIZE];
                    int echo_ret_len = make_echo(echo_msg, sizeof(echo_msg), (const char *)decrypted);

                    uint8_t response_hash;
                    uint8_t encrypted_response_hash;
                    response_hash = compute_hash(echo_msg, echo_ret_len);
                    response_hash &= 0x3F;
                    int encrypt_response_hash_rc = encrypt(server_key, &encrypted_response_hash, &response_hash, 1);
                    if (encrypt_response_hash_rc == RC_INVALID_ARGS) {
                        printf("[ERROR] Invalid arguments when encrypting hash.\n\n");
                        pdu->header.msg_type = MSG_ERROR;
                        pdu->header.direction = DIR_RESPONSE;
                        memset(pdu->payload, 0, in_len);  
                        send_all(client_socket, pdu_buff, sizeof(crypto_msg_t) + in_len);
                    } else if (encrypt_response_hash_rc == RC_INVALID_TEXT) {
                        printf("[ERROR] Invalid key when encrypting hash.\n\n");
                        pdu->header.msg_type = MSG_ERROR;
                        pdu->header.direction = DIR_RESPONSE;
                        memset(pdu->payload, 0, in_len);  
                        send_all(client_socket, pdu_buff, sizeof(crypto_msg_t) + in_len);
                    }

                    int encrypted_length = encrypt_string(server_key, encrypted, (uint8_t*)echo_msg, (size_t)echo_ret_len);
                    if (encrypted_length > 0) {
                        pdu->payload[0] = encrypted_response_hash;
                        memcpy(pdu->payload+1, encrypted, encrypted_length);
                        pdu->header.payload_len = encrypted_length;
                    }

                    pdu->header.msg_type = MSG_DIG_SIGNATURE;
                    pdu->header.direction = DIR_RESPONSE;
                    pdu->header.payload_len = htons((uint16_t)(encrypted_length + 1));
                    size_t total_pdu_echo_len = sizeof(crypto_pdu_t) + 1 + (size_t)encrypted_length;

                    ssize_t bytes_sent = send_all(client_socket, pdu_buff, total_pdu_echo_len);
                    (void)bytes_sent;
                    continue;
                }
                case MSG_CMD_SERVER_STOP:
                    pdu->header.direction = DIR_RESPONSE;
                    pdu->header.msg_type = MSG_SHUTDOWN;
                    ssize_t bytes_sent = send_all(client_socket, pdu_buff, (size_t)bytes_read);
                    (void)bytes_sent;
                    printf("Shutting down server...\n");
                    return 10;
                default:
                    pdu->header.msg_type = MSG_DATA;
                    pdu->header.direction = DIR_RESPONSE;
                    break;
            }

            char echo_msg[MAX_MSG_DATA_SIZE];
            int echo_ret_len = make_echo(echo_msg, sizeof(echo_msg), (const char *)msg_str);

            pdu->header.direction   = DIR_RESPONSE;
            pdu->header.payload_len = htons((uint16_t)echo_ret_len);
            memcpy(pdu->payload, echo_msg, (size_t)echo_ret_len);
            size_t total_pdu_echo_len = sizeof(crypto_pdu_t) + (size_t)echo_ret_len;

            ssize_t bytes_sent = send_all(client_socket, pdu_buff, total_pdu_echo_len);
            (void)bytes_sent;
            continue;

        } else if (bytes_read == 0) {
            printf("Client disconnected...\n");
            break;
        } else {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            printf("Client disconnected...\n");
            close(client_socket);
            return RC_CLIENT_EXITED;
        }
    }

    return 0;
}


static int extract_crypto_msg_data(const uint8_t* pdu_buff, uint16_t pdu_len, char* msg_str, uint16_t max_str_len, uint8_t* out_type, uint8_t* out_dir) {
    if (!pdu_buff || !msg_str || max_str_len == 0) {
        return -1;
    }

    if (pdu_len < sizeof(crypto_pdu_t)) {
        return -1;
    }

    const crypto_msg_t *pdu = (const crypto_msg_t *)pdu_buff;
    
    uint16_t msg_len = ntohs(pdu->header.payload_len);

    if (pdu_len != (uint16_t)(sizeof(crypto_pdu_t) + msg_len)) {
        return -1;
    }

    if (out_type) {
        *out_type = pdu->header.msg_type;
    }

    if (out_dir) {
        *out_dir = pdu->header.direction;
    }
    
    
    uint16_t copy_len = (msg_len < (uint16_t)(max_str_len - 1)) ? msg_len : (uint16_t)(max_str_len - 1);
    
    memcpy(msg_str, pdu->payload, copy_len);
    msg_str[copy_len] = '\0';
    
    return 0;
}

static uint8_t compute_hash(const char *message, size_t len) {
    uint8_t hash = 0;
    for (size_t i = 0; i < len; i++) {
        hash ^= message[i];
        hash = (hash << 1) | (hash >> 7);
    }
    return hash;
}

static ssize_t send_all(int sockfd, const char* buffer, size_t length) {
    size_t bytes_sent = 0;
    ssize_t result;
    
    while (bytes_sent < length) {
        result = send(sockfd, buffer + bytes_sent, length - bytes_sent, 0);
        if (result < 0) {
            return -1;
        }
        bytes_sent += result;
    }
    
    return bytes_sent;
}


static int make_echo(char* dst, size_t dst_sz, const char* src) {
    int n = snprintf(dst, dst_sz, "echo %s", src);
    if (n < 0) n = 0;
    if (n >= (int)dst_sz) n = (int)dst_sz - 1;
    return n;
}
