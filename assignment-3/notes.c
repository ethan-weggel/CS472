/**
 * =============================================================================
 * STUDENT ASSIGNMENT: CRYPTO-CLIENT.C
 * =============================================================================
 * 
 * ASSIGNMENT OBJECTIVE:
 * Implement a TCP client that communicates with a server using an encrypted
 * protocol. Your focus is on socket programming and network communication.
 * The cryptographic functions are provided for you in crypto-lib.
 * 
 * =============================================================================
 * WHAT YOU NEED TO IMPLEMENT:
 * =============================================================================
 * 
 * 1. SOCKET CONNECTION (start_client function):
 *    - Create a TCP socket using socket()
 *    - Configure the server address structure (struct sockaddr_in)
 *    - Connect to the server using connect()
 *    - Handle connection errors appropriately
 *    - Call your communication loop function
 *    - Close the socket when done
 * 
 * 2. CLIENT COMMUNICATION LOOP:
 *    - Create a function that handles the request/response cycle
 *    - Allocate buffers for sending and receiving data
 *    - Maintain a session key (crypto_key_t) for encryption
 *    - Loop to:
 *      a) Get user command using get_command() (provided below)
 *      b) Build a PDU (Protocol Data Unit) from the command
 *      c) Send the PDU to the server using send()
 *      d) Receive the server's response using recv()
 *      e) Process the response (extract key, decrypt data, etc.)
 *      f) Handle exit commands and connection closures
 *    - Free allocated buffers before returning
 * 
 * 3. PDU CONSTRUCTION:
 *    - Consider creating a helper function to build PDUs
 *    - Fill in the PDU header (msg_type, direction, payload_len)
 *    - For MSG_DATA: copy plaintext to payload
 *    - For MSG_ENCRYPTED_DATA: use encrypt_string() to encrypt before copying
 *    - For MSG_KEY_EXCHANGE: no payload needed
 *    - For command messages: no payload needed
 *    - Return the total PDU size (header + payload)
 * 
 * =============================================================================
 * ONE APPROACH TO SOLVE THIS PROBLEM:
 * =============================================================================
 * 
 * FUNCTION STRUCTURE:
 * 
 * void start_client(const char* addr, int port) {
 *     // 1. Create TCP socket
 *     // 2. Configure server address (sockaddr_in)
 *     // 3. Connect to server
 *     // 4. Print connection confirmation
 *     // 5. Call your communication loop function
 *     // 6. Close socket
 *     // 7. Print disconnection message
 * }
 * 
 * int client_loop(int socket_fd) {
 *     // 1. Allocate buffers (send, receive, input)
 *     // 2. Initialize session_key to NULL_CRYPTO_KEY
 *     // 3. Loop:
 *     //    a) Call get_command() to get user input
 *     //    b) Build PDU from command (use helper function)
 *     //    c) Send PDU using send()
 *     //    d) If exit command, break after sending
 *     //    e) Receive response using recv()
 *     //    f) Handle recv() return values (0 = closed, <0 = error)
 *     //    g) Process response:
 *     //       - If MSG_KEY_EXCHANGE: extract key from payload
 *     //       - If MSG_ENCRYPTED_DATA: decrypt using decrypt_string()
 *     //       - Print results
 *     //    h) Loop back
 *     // 4. Free buffers
 *     // 5. Return success/error code
 * }
 * 
 * int build_packet(const msg_cmd_t *cmd, crypto_msg_t *pdu, crypto_key_t key) {
 *     // 1. Set pdu->header.msg_type = cmd->cmd_id
 *     // 2. Set pdu->header.direction = DIR_REQUEST
 *     // 3. Based on cmd->cmd_id:
 *     //    - MSG_DATA: copy cmd->cmd_line to payload, set length
 *     //    - MSG_ENCRYPTED_DATA: encrypt cmd->cmd_line, set length
 *     //    - MSG_KEY_EXCHANGE: set length to 0
 *     //    - Command messages: set length to 0
 *     // 4. Return sizeof(crypto_pdu_t) + payload_len
 * }
 * 
 * =============================================================================
 * IMPORTANT PROTOCOL DETAILS:
 * =============================================================================
 * 
 * PDU STRUCTURE:
 *   typedef struct crypto_pdu {
 *       uint8_t  msg_type;      // MSG_DATA, MSG_ENCRYPTED_DATA, etc.
 *       uint8_t  direction;     // DIR_REQUEST or DIR_RESPONSE
 *       uint16_t payload_len;   // Length of payload in bytes
 *   } crypto_pdu_t;
 * 
 *   typedef struct crypto_msg {
 *       crypto_pdu_t header;
 *       uint8_t      payload[]; // Flexible array
 *   } crypto_msg_t;
 * 
 * MESSAGE TYPES (from protocol.h):
 *   MSG_KEY_EXCHANGE     - Request/send encryption key
 *   MSG_DATA             - Plain text message
 *   MSG_ENCRYPTED_DATA   - Encrypted message (requires session key)
 *   MSG_CMD_CLIENT_STOP  - Client exit command
 *   MSG_CMD_SERVER_STOP  - Server shutdown command
 * 
 * TYPICAL MESSAGE FLOW:
 *   1. Client sends MSG_KEY_EXCHANGE request
 *   2. Server responds with MSG_KEY_EXCHANGE + key in payload
 *   3. Client extracts key: memcpy(&session_key, response->payload, sizeof(crypto_key_t))
 *   4. Client can now send MSG_ENCRYPTED_DATA
 *   5. Server responds with MSG_ENCRYPTED_DATA
 *   6. Client decrypts using decrypt_string()
 * 
 * =============================================================================
 * CRYPTO LIBRARY FUNCTIONS YOU'LL USE:
 * =============================================================================
 * 
 * int encrypt_string(crypto_key_t key, uint8_t *out, uint8_t *in, size_t len)
 *   - Encrypts a string before sending
 *   - Returns number of encrypted bytes or negative on error
 * 
 * int decrypt_string(crypto_key_t key, uint8_t *out, uint8_t *in, size_t len)
 *   - Decrypts received data
 *   - Returns number of decrypted chars or negative on error
 *   - NOTE: Output is NOT null-terminated, you must add '\0'
 * 
 * void print_msg_info(crypto_msg_t *msg, crypto_key_t key, int mode)
 *   - Prints PDU details for debugging
 *   - Use CLIENT_MODE for the mode parameter
 *   - VERY helpful for debugging your protocol!
 * 
 * =============================================================================
 * DEBUGGING TIPS:
 * =============================================================================
 * 
 * 1. Use print_msg_info() before sending and after receiving
 * 2. Check return values from ALL socket operations
 * 3. Verify payload_len matches actual data length
 * 4. Remember: recv() may return less bytes than expected
 * 5. Encrypted data requires a valid session key (check for NULL_CRYPTO_KEY)
 * 6. Use printf() liberally to trace program flow
 * 
 * =============================================================================
 * TESTING RECOMMENDATIONS:
 * =============================================================================
 * 
 * 1. Start simple: Get plain MSG_DATA working first
 * 2. Test key exchange: Send '#' command
 * 3. Test encryption: Send '!message' after key exchange
 * 4. Test exit commands: '-' for client exit, '=' for server shutdown
 * 5. Test error cases: What if server closes unexpectedly?
 * 
 * Good luck! Remember: Focus on the socket operations. The crypto is done!
 * =============================================================================
 */