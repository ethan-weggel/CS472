/**
 * =============================================================================
 * STUDENT ASSIGNMENT: CRYPTO-SERVER.C
 * =============================================================================
 * 
 * ASSIGNMENT OBJECTIVE:
 * Implement a TCP server that accepts client connections and processes
 * encrypted/plaintext messages. Your focus is on socket programming, connection
 * handling, and the server-side protocol implementation.
 * 
 * =============================================================================
 * WHAT YOU NEED TO IMPLEMENT:
 * =============================================================================
 * 
 * 1. SERVER SOCKET SETUP (start_server function):
 *    - Create a TCP socket using socket()
 *    - Set SO_REUSEADDR socket option (helpful during development)
 *    - Configure server address structure (struct sockaddr_in)
 *    - Bind the socket to the address using bind()
 *    - Start listening with listen()
 *    - Call your server loop function
 *    - Close socket on shutdown
 * 
 * 2. SERVER MAIN LOOP:
 *    - Create a function that handles multiple clients sequentially
 *    - Loop to:
 *      a) Accept incoming connections using accept()
 *      b) Get client's IP address for logging (inet_ntop)
 *      c) Call your client service function
 *      d) Close the client socket when done
 *      e) Return to accept next client (or exit if shutdown requested)
 * 
 * 3. CLIENT SERVICE LOOP:
 *    - Create a function that handles communication with ONE client
 *    - Allocate buffers for sending and receiving
 *    - Maintain session keys (client_key and server_key)
 *    - Loop to:
 *      a) Receive a PDU from the client using recv()
 *      b) Handle recv() return values (0 = closed, <0 = error)
 *      c) Parse the received PDU
 *      d) Check for special commands (exit, server shutdown)
 *      e) Build response PDU based on message type
 *      f) Send response using send()
 *      g) Return appropriate status code when client exits
 *    - Free buffers before returning
 * 
 * 4. RESPONSE BUILDING:
 *    - Consider creating a helper function to build response PDUs
 *    - Handle different message types:
 *      * MSG_KEY_EXCHANGE: Call gen_key_pair(), send client_key to client
 *      * MSG_DATA: Echo back with "echo " prefix
 *      * MSG_ENCRYPTED_DATA: Decrypt, add "echo " prefix, re-encrypt
 *      * MSG_CMD_CLIENT_STOP: No response needed (client will exit)
 *      * MSG_CMD_SERVER_STOP: No response needed (server will exit)
 *    - Set proper direction (DIR_RESPONSE)
 *    - Return total PDU size
 * 
 * =============================================================================
 * ONE APPROACH TO SOLVE THIS PROBLEM:
 * =============================================================================
 * 
 * FUNCTION STRUCTURE:
 * 
 * void start_server(const char* addr, int port) {
 *     // 1. Create TCP socket
 *     // 2. Set SO_REUSEADDR option (for development)
 *     // 3. Configure server address (sockaddr_in)
 *     //    - Handle "0.0.0.0" specially (use INADDR_ANY)
 *     // 4. Bind socket to address
 *     // 5. Start listening (use BACKLOG constant)
 *     // 6. Call your server loop function
 *     // 7. Close socket
 * }
 * 
 * int server_loop(int server_socket, const char* addr, int port) {
 *     // 1. Print "Server listening..." message
 *     // 2. Infinite loop:
 *     //    a) Accept connection (creates new client socket)
 *     //    b) Get client IP using inet_ntop()
 *     //    c) Print "Client connected..." message
 *     //    d) Call service_client_loop(client_socket)
 *     //    e) Check return code:
 *     //       - RC_CLIENT_EXITED: close socket, accept next client
 *     //       - RC_CLIENT_REQ_SERVER_EXIT: close sockets, return
 *     //       - Error: close socket, continue
 *     //    f) Close client socket
 *     // 3. Return when server shutdown requested
 * }
 * 
 * int service_client_loop(int client_socket) {
 *     // 1. Allocate send/receive buffers
 *     // 2. Initialize keys to NULL_CRYPTO_KEY
 *     // 3. Loop:
 *     //    a) Receive PDU from client
 *     //    b) Check recv() return:
 *     //       - 0: client closed, return RC_CLIENT_EXITED
 *     //       - <0: error, return RC_CLIENT_EXITED
 *     //    c) Cast buffer to crypto_msg_t*
 *     //    d) Check for MSG_CMD_SERVER_STOP -> return RC_CLIENT_REQ_SERVER_EXIT
 *     //    e) Build response PDU (use helper function)
 *     //    f) Send response
 *     //    g) Loop back
 *     // 4. Free buffers before returning
 * }
 * 
 * int build_response(crypto_msg_t *request, crypto_msg_t *response, 
 *                    crypto_key_t *client_key, crypto_key_t *server_key) {
 *     // 1. Set response->header.direction = DIR_RESPONSE
 *     // 2. Set response->header.msg_type = request->header.msg_type
 *     // 3. Switch on request type:
 *     //    MSG_KEY_EXCHANGE:
 *     //      - Call gen_key_pair(server_key, client_key)
 *     //      - Copy client_key to response->payload
 *     //      - Set payload_len = sizeof(crypto_key_t)
 *     //    MSG_DATA:
 *     //      - Format: "echo <original message>"
 *     //      - Copy to response->payload
 *     //      - Set payload_len
 *     //    MSG_ENCRYPTED_DATA:
 *     //      - Decrypt request->payload using decrypt_string()
 *     //      - Format: "echo <decrypted message>"
 *     //      - Encrypt result using encrypt_string()
 *     //      - Copy encrypted data to response->payload
 *     //      - Set payload_len
 *     //    MSG_CMD_*:
 *     //      - Set payload_len = 0
 *     // 4. Return sizeof(crypto_pdu_t) + payload_len
 * }
 * 
 * =============================================================================
 * IMPORTANT PROTOCOL DETAILS:
 * =============================================================================
 * 
 * SERVER RESPONSIBILITIES:
 * 1. Generate encryption keys when client requests (MSG_KEY_EXCHANGE)
 * 2. Send the CLIENT'S key to the client (not the server's key!)
 * 3. Keep both keys: server_key (for decrypting client messages)
 *                    client_key (to send to client)
 * 4. Echo messages back with "echo " prefix
 * 5. Handle encrypted data: decrypt -> process -> encrypt -> send
 * 
 * KEY GENERATION:
 *   crypto_key_t server_key, client_key;
 *   gen_key_pair(&server_key, &client_key);
 *   // Send client_key to the client in MSG_KEY_EXCHANGE response
 *   memcpy(response->payload, &client_key, sizeof(crypto_key_t));
 * 
 * DECRYPTING CLIENT DATA:
 *   // Client encrypted with their key, we decrypt with server_key
 *   uint8_t decrypted[MAX_SIZE];
 *   decrypt_string(server_key, decrypted, request->payload, request->header.payload_len);
 *   decrypted[request->header.payload_len] = '\0'; // Null-terminate
 * 
 * ENCRYPTING RESPONSE:
 *   // We encrypt with server_key for client to decrypt with their key
 *   uint8_t encrypted[MAX_SIZE];
 *   int encrypted_len = encrypt_string(server_key, encrypted, plaintext, plaintext_len);
 *   memcpy(response->payload, encrypted, encrypted_len);
 *   response->header.payload_len = encrypted_len;
 * 
 * RETURN CODES:
 *   RC_CLIENT_EXITED          - Client disconnected normally
 *   RC_CLIENT_REQ_SERVER_EXIT - Client requested server shutdown
 *   RC_OK                     - Success
 *   Negative values           - Errors
 * 
 * =============================================================================
 * SOCKET PROGRAMMING REMINDERS:
 * =============================================================================
 * 
 * CREATING AND BINDING:
 *   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
 *   
 *   struct sockaddr_in addr;
 *   memset(&addr, 0, sizeof(addr));
 *   addr.sin_family = AF_INET;
 *   addr.sin_port = htons(port);
 *   addr.sin_addr.s_addr = INADDR_ANY;  // or use inet_pton()
 *   
 *   bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
 *   listen(sockfd, BACKLOG);
 * 
 * ACCEPTING CONNECTIONS:
 *   struct sockaddr_in client_addr;
 *   socklen_t addr_len = sizeof(client_addr);
 *   int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
 * 
 * GETTING CLIENT IP:
 *   char client_ip[INET_ADDRSTRLEN];
 *   inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
 * 
 * =============================================================================
 * DEBUGGING TIPS:
 * =============================================================================
 * 
 * 1. Use print_msg_info() to display received and sent PDUs
 * 2. Print client IP when connections are accepted
 * 3. Check all socket operation return values
 * 4. Test with plaintext (MSG_DATA) before trying encryption
 * 5. Verify keys are generated correctly (print key values)
 * 6. Use telnet or netcat to test basic connectivity first
 * 7. Handle partial recv() - though for this assignment, assume full PDU arrives
 * 
 * =============================================================================
 * TESTING RECOMMENDATIONS:
 * =============================================================================
 * 
 * 1. Start simple: Accept connection and echo plain text
 * 2. Test key exchange: Client sends '#', server generates and returns key
 * 3. Test encryption: Client sends '!message', server decrypts, echoes, encrypts
 * 4. Test multiple clients: Connect, disconnect, connect again
 * 5. Test shutdown: Client sends '=', server exits gracefully
 * 6. Test error cases: Client disconnects unexpectedly
 * 
 * Good luck! Server programming requires careful state management!
 * =============================================================================
 */

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

static int extract_crypto_msg_data(const uint8_t* pdu_buff, uint16_t pdu_len, char* msg_str, uint16_t max_str_len, uint8_t* out_type, uint8_t* out_dir);
static int crypto_pdu_from_cstr(const char* msg_str, uint8_t* pdu_buff, uint16_t pdu_buff_sz, uint8_t msg_type, uint8_t direction);


/* =============================================================================
 * STUDENT TODO: IMPLEMENT THIS FUNCTION
 * =============================================================================
 * This is the main server initialization function. You need to:
 * 1. Create a TCP socket
 * 2. Set socket options (SO_REUSEADDR)
 * 3. Bind to the specified address and port
 * 4. Start listening for connections
 * 5. Call your server loop function
 * 6. Clean up when done
 * 
 * Parameters:
 *   addr - Server bind address (e.g., "0.0.0.0" for all interfaces)
 *   port - Server port number (e.g., 1234)
 * 
 * NOTE: If addr is "0.0.0.0", use INADDR_ANY instead of inet_pton()
 */

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
                        send(client_socket, pdu_buff, sizeof(crypto_pdu_t) + sizeof(err_str), 0);
                        continue;
                    }
                    send(client_socket, pdu_buff, sizeof(crypto_pdu_t) + sizeof(crypto_key_t), 0);
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
                    int echo_ret_len = snprintf(echo_msg, sizeof(echo_msg), "echo %s", decrypted);
                    if (echo_ret_len < 0) {
                        echo_ret_len = 0;
                    }
                    if (echo_ret_len >= (int)sizeof(echo_msg)) {
                        echo_ret_len = (int)sizeof(echo_msg) - 1;
                    }

                    int encrypted_length = encrypt_string(server_key, encrypted, (uint8_t*)echo_msg, (size_t)echo_ret_len);
                    if (encrypted_length > 0) {
                        memcpy(pdu->payload, encrypted, encrypted_length);
                        pdu->header.payload_len = encrypted_length;
                    }

                    pdu->header.msg_type = MSG_ENCRYPTED_DATA;
                    pdu->header.direction = DIR_RESPONSE;
                    pdu->header.payload_len = htons((uint16_t)encrypted_length);
                    size_t total_pdu_echo_len = sizeof(crypto_pdu_t) + (size_t)encrypted_length;

                    ssize_t bytes_sent = send(client_socket, pdu_buff, total_pdu_echo_len, 0);
                    (void)bytes_sent;
                    continue;
                }
                case MSG_CMD_SERVER_STOP:
                    pdu->header.direction = DIR_RESPONSE;
                    pdu->header.msg_type = MSG_SHUTDOWN;
                    ssize_t bytes_sent = send(client_socket, pdu_buff, (size_t)bytes_read, 0);
                    (void)bytes_sent;
                    printf("Shutting down server...\n");
                    return 10;
                default:
                    pdu->header.msg_type = MSG_DATA;
                    pdu->header.direction = DIR_RESPONSE;
                    break;
            }

            char echo_msg[MAX_MSG_DATA_SIZE];
            int echo_ret_len = snprintf(echo_msg, sizeof(echo_msg), "echo %s", msg_str);
            if (echo_ret_len < 0) {
                echo_ret_len = 0;
            }
            if (echo_ret_len >= (int)sizeof(echo_msg)) {
                echo_ret_len = (int)sizeof(echo_msg) - 1;
            }

            pdu->header.direction   = DIR_RESPONSE;
            pdu->header.payload_len = htons((uint16_t)echo_ret_len);
            memcpy(pdu->payload, echo_msg, (size_t)echo_ret_len);
            size_t total_pdu_echo_len = sizeof(crypto_pdu_t) + (size_t)echo_ret_len;

            ssize_t bytes_sent = send(client_socket, pdu_buff, total_pdu_echo_len, 0);
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
            perror("recv");
            close(client_socket);
            return RC_CLIENT_EXITED;
        }
    }

    return 0;
}

int build_response(crypto_msg_t *request, crypto_msg_t *response, 
                   crypto_key_t *client_key, crypto_key_t *server_key) {

    (void)request;
    (void)response;
    (void)client_key;
    (void)server_key;

    return RC_OK; 
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
