#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include "crypto-client.h"
#include "crypto-lib.h"
#include "protocol.h"
#include <inttypes.h>

static int extract_crypto_msg_data(const uint8_t* pdu_buff, uint16_t pdu_len, char* msg_str, uint16_t max_str_len, uint8_t* out_type, uint8_t* out_dir);
static int crypto_pdu_from_cstr(const char* msg_str, uint8_t* pdu_buff, uint16_t pdu_buff_sz, uint8_t msg_type, uint8_t direction);
static ssize_t recv_all(int fd, uint8_t *buf, size_t want);
static uint8_t compute_hash(const char *message, size_t len);
static void print_msg_info_with_signature(crypto_msg_t *msg, crypto_key_t key, int mode, uint8_t signature_byte, int verified);
static ssize_t send_all(int sockfd, uint8_t* buffer, size_t length);
static void print_msg_like(crypto_msg_t *wire, crypto_key_t key, int mode);


void start_client(const char* addr, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, addr, &client_addr.sin_addr) <= 0) {
        close(sockfd);
        printf("Error converting address to network format...\n");
        return;
    }

    if (connect(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        printf("Server not running...\n");
        close(sockfd);
        return;
    }

    printf("connected to server %s:%d\n", addr, port);
    printf("Type messages to send to server.\n");
    printf("Type 'exit' to quit, or 'exit server' to shutdown the server.\n");
    printf("Press Ctrl+C to exit at any time.\n\n");

    int rc = client_loop(sockfd);

    printf("Client exited with return code %d\n", rc);
    return;
}

int client_loop(int sockfd) {
    uint8_t send_buff[MAX_MSG_SIZE];
    uint8_t recv_buff[MAX_MSG_SIZE];
    char msg[MAX_MSG_DATA_SIZE];
    char input[MAX_MSG_DATA_SIZE];

    crypto_key_t session_key = NULL_CRYPTO_KEY;

    while (1) {
        msg_cmd_t command;
        int did_send = 0;

        int result = get_command(input, MAX_MSG_DATA_SIZE, &command);
        if (result == CMD_EXECUTE) {

            if (!command.cmd_id) {
                continue; 
            }

            if (command.cmd_line != NULL && command.cmd_line[0] == '\0') {
                continue;
            }

            if (command.cmd_line && strcmp(command.cmd_line, "exit") == 0) {
                command.cmd_id = MSG_CMD_CLIENT_STOP;
                command.cmd_line = NULL;
            } else if (command.cmd_line && strcmp(command.cmd_line, "exit server") == 0) {
                command.cmd_id = MSG_CMD_SERVER_STOP;
                command.cmd_line = NULL;
            }

            switch (command.cmd_id) {
                case MSG_KEY_EXCHANGE: {
                    did_send = 1;
                    int size_of_pdu = crypto_pdu_from_cstr(command.cmd_line, send_buff, MAX_MSG_SIZE, command.cmd_id, DIR_REQUEST);
                    crypto_msg_t *wire = (crypto_msg_t*)send_buff;

                    print_msg_like(wire, session_key, CLIENT_MODE);

                    send_all(sockfd, send_buff, size_of_pdu);
                    break;
                } 
                case MSG_ENCRYPTED_DATA: {

                    did_send = 1;
                    int size_of_pdu = crypto_pdu_from_cstr(command.cmd_line, send_buff, MAX_MSG_SIZE, command.cmd_id, DIR_REQUEST);
                    crypto_msg_t* wire = (crypto_msg_t*)send_buff;
                    

                    if (session_key == NULL_CRYPTO_KEY) {
                        printf("[ERROR] No session key established. Cannot send encrypted data.\n\n");
                        did_send = 0;
                        continue;
                    } else {
                        uint8_t encrypted[MAX_MSG_DATA_SIZE];
                        int encrypted_length = encrypt_string(session_key, encrypted, (uint8_t*)command.cmd_line, strlen(command.cmd_line));
                        if (encrypted_length > 0) {
                            memcpy(wire->payload, encrypted, (size_t)encrypted_length);
                            wire->header.payload_len = htons((uint16_t)encrypted_length);
                            size_of_pdu = (int)(sizeof(crypto_pdu_t) + (size_t)encrypted_length);   
                        }
                    }

                    print_msg_like(wire, session_key, CLIENT_MODE);
                    
                    send_all(sockfd, send_buff, size_of_pdu);
                    break;
                }
                case MSG_DIG_SIGNATURE: {
                    did_send = 1;
                    int size_of_pdu = crypto_pdu_from_cstr(command.cmd_line, send_buff, MAX_MSG_SIZE, command.cmd_id, DIR_REQUEST);
                    crypto_msg_t* wire = (crypto_msg_t*)send_buff;
                    uint16_t msg_len = ntohs(wire->header.payload_len);
                    uint8_t encrypted_hash;
                    uint8_t encrypted_msg[MAX_MSG_DATA_SIZE];
                    int encrypted_length;
                    int hash;

                    if (session_key == NULL_CRYPTO_KEY) {
                        printf("[ERROR] No session key established. Cannot send encrypted data.\n\n");
                        did_send = 0;
                        continue;
                    } else {
                        // encrypt payload
                        encrypted_length = encrypt_string(session_key, encrypted_msg, (uint8_t*)command.cmd_line, strlen(command.cmd_line));
                        
                        // encrypt hash
                        hash = compute_hash(command.cmd_line, strlen(command.cmd_line));
                        hash &= 0x3F;
                        int encrypt_hash_rc = encrypt(session_key, &encrypted_hash, &hash, 1);
                        if (encrypt_hash_rc == RC_INVALID_ARGS) {
                            printf("[ERROR] Invalid arguments when encrypting hash.\n\n");
                        } else if (encrypt_hash_rc == RC_INVALID_TEXT) {
                            printf("[ERROR] Invalid key when encrypting hash.\n\n");
                        }
                        if (encrypted_length > 0) {
                            memcpy(wire->payload + 1, encrypted_msg, (size_t)encrypted_length);
                            wire->header.payload_len = htons((uint16_t)(encrypted_length + 1));
                            size_of_pdu = (int)(sizeof(crypto_pdu_t) + 1 + (size_t)encrypted_length);   
                        }
                    }

                    wire->header.msg_type = MSG_DIG_SIGNATURE;
                    wire->header.direction = DIR_REQUEST;
                    wire->payload[0] = encrypted_hash;
                    memcpy(&wire->payload[1], encrypted_msg, encrypted_length);

                    // request printing
                    uint8_t tmp_out[sizeof(crypto_pdu_t) + MAX_MSG_DATA_SIZE];
                    crypto_msg_t *print_out = (crypto_msg_t*)tmp_out;
                    print_out->header.msg_type = wire->header.msg_type;
                    print_out->header.direction = wire->header.direction;
                    print_out->header.payload_len = encrypted_length + 1; 
                    memcpy(print_out->payload, wire->payload, msg_len);
                    print_out->payload[msg_len] = '\0';
                    print_msg_info_with_signature(print_out, session_key, CLIENT_MODE, hash, 0);
                    
                    send_all(sockfd, send_buff, size_of_pdu);
                    break;
                }
                case MSG_HELP_CMD:
                    did_send = 0;
                    break;
                case MSG_CMD_CLIENT_STOP: {
                    // sends data 
                    int size_of_pdu = crypto_pdu_from_cstr(command.cmd_line, send_buff, MAX_MSG_SIZE, command.cmd_id, DIR_REQUEST);
                    crypto_msg_t* wire = (crypto_msg_t*)send_buff;

                    print_msg_like(wire, session_key, CLIENT_MODE);

                    send_all(sockfd, send_buff, size_of_pdu);
                    did_send = 0;
                    return 0;
                }
                default: {
                    // sends data
                    int size_of_pdu = crypto_pdu_from_cstr(command.cmd_line, send_buff, MAX_MSG_SIZE, command.cmd_id, DIR_REQUEST);
                    crypto_msg_t *wire = (crypto_msg_t*)send_buff;

                    print_msg_like(wire, session_key, CLIENT_MODE);

                    send_all(sockfd, send_buff, size_of_pdu);
                    did_send = 1;
                    break;
                }
            }

            if (!did_send) {
                continue;
            }

            // receives header
            uint8_t msg_type, direction;
            uint16_t payload_length;

            ssize_t bytes_received = recv_all(sockfd, recv_buff, sizeof(crypto_pdu_t));
            if (bytes_received <= 0) {
                close(sockfd);
                return -1;
            }

            memcpy(&payload_length, recv_buff + 2, 2);
            payload_length = ntohs(payload_length);

            if (payload_length > 0) {
                ssize_t pr = recv_all(sockfd, recv_buff + sizeof(crypto_pdu_t), payload_length);
                if (pr <= 0) {
                    close(sockfd);
                    return -1;
                }
            }


            extract_crypto_msg_data(recv_buff, (sizeof(crypto_pdu_t) + payload_length), msg, MAX_MSG_DATA_SIZE, &msg_type, &direction);
            crypto_msg_t *wire_in = (crypto_msg_t*)recv_buff;
            uint16_t msg_len_in = ntohs(wire_in->header.payload_len);

            switch (wire_in->header.msg_type) {
                case MSG_KEY_EXCHANGE: {
                    if (payload_length >= sizeof(crypto_key_t)) {
                        memcpy(&session_key, wire_in->payload, sizeof(crypto_key_t));
                    }
                    break;
                }
                case MSG_ENCRYPTED_DATA: {
                    uint8_t decrypted[MAX_MSG_DATA_SIZE];
                    int decrypted_length = decrypt_string(session_key, decrypted, wire_in->payload, msg_len_in);
                    if (decrypted_length > 0) {
                        decrypted[decrypted_length] = '\0';
                    }
                    break;
                }
                case MSG_DIG_SIGNATURE: {

                    uint8_t encrypted_sig = wire_in->payload[0];
                    uint8_t received_hash;
                    decrypt(session_key, &received_hash, &encrypted_sig, 1);
                    

                    uint8_t decrypted[MAX_MSG_DATA_SIZE];
                    int decrypted_length = decrypt_string(session_key, decrypted, wire_in->payload+1, msg_len_in-1);
                    if (decrypted_length > 0) {
                        decrypted[decrypted_length] = '\0';
                    }

                    uint8_t computed_hash = compute_hash((const char *)decrypted, (size_t)decrypted_length);
                    computed_hash &= 0x3F;

                    if (received_hash != computed_hash) {
                        printf("[WARNING] Server response signature invalid!\n");
                    } else {
                        uint8_t tmp_in[sizeof(crypto_pdu_t) + MAX_MSG_DATA_SIZE];
                        crypto_msg_t *print_in = (crypto_msg_t*)tmp_in;
                        print_in->header.msg_type = wire_in->header.msg_type;
                        print_in->header.direction = wire_in->header.direction;
                        print_in->header.payload_len = msg_len_in;
                        memcpy(print_in->payload, wire_in->payload, msg_len_in);
                        print_in->payload[msg_len_in] = '\0';
                        print_msg_info_with_signature(print_in, session_key, CLIENT_MODE, computed_hash, 1);
                        continue;
                    }
                    break;
                }
                case MSG_EXIT:
                    return 0;
                case MSG_SHUTDOWN:
                    return 0;
                case MSG_DATA:
                    break;
                default:
                    continue;
            }

            print_msg_like(wire_in, session_key, CLIENT_MODE);

        } else {
            continue;
        }
    }
}


int get_command(char *cmd_buff, size_t cmd_buff_sz, msg_cmd_t *msg_cmd)
{
    if ((cmd_buff == NULL) || (cmd_buff_sz == 0)) return CMD_NO_EXEC;

    printf("> ");
    fflush(stdout);
    
    // Get input from user
    if (fgets(cmd_buff, cmd_buff_sz, stdin) == NULL) {
        printf("[WARNING] Error reading input command.\n\n");
        return CMD_NO_EXEC;
    }
    
    // Remove trailing newline
    cmd_buff[strcspn(cmd_buff, "\n")] = '\0';

    // Interpret the command based on first character
    switch (cmd_buff[0]) {
        case '!':
            // Encrypted message - everything after '!' is the message
            msg_cmd->cmd_id = MSG_ENCRYPTED_DATA;
            msg_cmd->cmd_line = cmd_buff + 1; // Skip the '!' character
            return CMD_EXECUTE;
            
        case '#':
            // Key exchange request - no message data
            msg_cmd->cmd_id = MSG_KEY_EXCHANGE;
            msg_cmd->cmd_line = NULL;
            return CMD_EXECUTE;
            
        case '$':
            // Digital signature (I did implement this!)
            msg_cmd->cmd_id = MSG_DIG_SIGNATURE;
            msg_cmd->cmd_line = cmd_buff + 1;;
            return CMD_EXECUTE;
            
        case '-':
            // Client exit command
            msg_cmd->cmd_id = MSG_CMD_CLIENT_STOP;
            msg_cmd->cmd_line = NULL;
            return CMD_EXECUTE;
            
        case '=':
            // Server shutdown command
            msg_cmd->cmd_id = MSG_CMD_SERVER_STOP;
            msg_cmd->cmd_line = NULL;
            return CMD_EXECUTE;
            
        case '?':
            // Help - display available commands
            msg_cmd->cmd_id = MSG_HELP_CMD;
            msg_cmd->cmd_line = NULL;
            printf("Available commands:\n");
            printf("  <message>  : Send plain text message\n");
            printf("  !<message> : Send encrypted message (requires key exchange first)\n");
            printf("  #          : Request key exchange from server\n");
            printf("  ?          : Show this help message\n");
            printf("  -          : Exit the client\n");
            printf("  =          : Exit the client and request server shutdown\n\n");
            return CMD_NO_EXEC;
            
        default:
            // Regular text message
            msg_cmd->cmd_id = MSG_DATA;
            msg_cmd->cmd_line = cmd_buff;
            return CMD_EXECUTE;
    }
    
    return CMD_NO_EXEC;
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

static int crypto_pdu_from_cstr(const char* msg_str, uint8_t* pdu_buff, uint16_t pdu_buff_sz, uint8_t msg_type, uint8_t direction) {
    if (!pdu_buff) {
        return -1;
    } 

    uint16_t msg_len = msg_str ? (uint16_t)strlen(msg_str) : 0;
    uint16_t total_len = (uint16_t)(sizeof(crypto_pdu_t) + msg_len);

    if (total_len > pdu_buff_sz) {
        return -1;
    }

    crypto_msg_t* pdu = (crypto_msg_t*)pdu_buff;

    pdu->header.msg_type = msg_type;
    pdu->header.direction = direction;
    pdu->header.payload_len = htons(msg_len);

    if (msg_len > 0) {
        memcpy(pdu->payload, msg_str, msg_len);
    }          

    return total_len;
}

static ssize_t recv_all(int fd, uint8_t *buf, size_t want) {
    size_t got = 0;
    while (got < want) {
        ssize_t n = recv(fd, buf + got, want - got, 0);
        if (n == 0) return 0;
        if (n < 0) {
            return -1;
        }
        got += (size_t)n;
    }
    return (ssize_t)got;
}

static uint8_t compute_hash(const char *message, size_t len) {
    uint8_t hash = 0;
    for (size_t i = 0; i < len; i++) {
        hash ^= message[i];
        hash = (hash << 1) | (hash >> 7);
    }
    return hash;
}

static void print_msg_info_with_signature(crypto_msg_t *msg, crypto_key_t key, int mode, uint8_t signature_byte, int verified) {
    if (msg == NULL) return;

    const crypto_pdu_t *pdu = &msg->header;

    if (!verified) {
        printf("[INFO] Computing signature...\n");
        printf("[INFO] Signature: 0x%02x\n", signature_byte);
    }

    if(pdu->direction == DIR_REQUEST) {
        printf(">>>>>>>>>>>>>>> REQUEST >>>>>>>>>>>>>>>\n");
    } else {
        printf("<<<<<<<<<<<<<<< RESPONSE <<<<<<<<<<<<<<<\n");
    }
    
    printf("-------------------------\nPDU Info:\n");
    
    printf("  Type: ");
    printf("DIGITAL_SIGNATURE");
    printf("\n");
    
    printf("  Direction: %s\n", 
        pdu->direction == DIR_REQUEST ? "REQUEST" : "RESPONSE");
    
    printf("  Payload Length: %u bytes\n", pdu->payload_len-1);

    if (pdu->payload_len > 0) {

        printf("  Signature byte: 0x%02x\n", signature_byte);
        if (key == NULL_CRYPTO_KEY) {
            printf("  Payload: Encrypted data but invalid key provided to decrypt\n");
            return;
        }
        uint8_t *msg_data = malloc(pdu->payload_len-1);
        if (printable_encrypted_string((uint8_t *)msg->payload, msg_data, pdu->payload_len-1) == RC_OK) {
            msg_data[pdu->payload_len] = '\0'; // Null-terminate
            printf("  Payload (encrypted): \"%s\"\n", msg_data);

            if ((mode == SERVER_MODE && pdu->direction == DIR_REQUEST) ||
                (mode == CLIENT_MODE && pdu->direction == DIR_RESPONSE)) {

                if(decrypt_string(key, msg_data, msg->payload + 1, pdu->payload_len-1) > 0) {
                    msg_data[pdu->payload_len] = '\0'; // Null-terminate
                    printf("  Payload (decrypted): \"%s\"\n", msg_data);
                } else {
                    printf("  Payload: Decryption error\n");
                }

            }

        } else {
            printf("  Payload: Invalid data\n");
        }
        free(msg_data);

        if (verified) {
            printf("✓ Server signature verified!\n");
        }

    } else {
        printf("  No Payload\n");
    }

    if(pdu->direction == DIR_REQUEST) {
        printf(">>>>>>>>>>>>> END REQUEST >>>>>>>>>>>>>\n\n");
    } else {
        printf("<<<<<<<<<<<<< END RESPONSE <<<<<<<<<<<<<\n\n");
    }
}

static ssize_t send_all(int sockfd, uint8_t* buffer, size_t length) {
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

static void print_msg_like(crypto_msg_t *wire, crypto_key_t key, int mode) {
    uint16_t msg_len = ntohs(wire->header.payload_len);
    uint8_t tmp[sizeof(crypto_pdu_t) + MAX_MSG_DATA_SIZE];
    crypto_msg_t *out = (crypto_msg_t *)tmp;
    out->header.msg_type = wire->header.msg_type;
    out->header.direction = wire->header.direction;
    out->header.payload_len = msg_len;
    if (msg_len > 0) {
        memcpy(out->payload, wire->payload, msg_len);
    }
    out->payload[msg_len] = '\0';
    print_msg_info(out, key, mode);
}
