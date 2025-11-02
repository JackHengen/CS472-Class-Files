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


void connect_to_clients(int listen_fd);
int talk_to_client(int client_sock);

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
   int fd = socket(AF_INET,SOCK_STREAM,0);

   int opt = 1;
   int opterr = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
   if(opterr == -1){
      perror("socket options failed");
      exit(-1);
   }

   struct sockaddr_in addr_struct;
   memset(&addr, 0, sizeof(addr_struct));
   addr_struct.sin_family = AF_INET;
   addr_struct.sin_port = htons(port);
   addr_struct.sin_addr.s_addr = INADDR_ANY; //parse addr here and if 0.0.0.0 then use INADDR_ANY - may need to use inet_pton

   int binderr = bind(fd, (struct sockaddr*)&addr_struct, sizeof(addr_struct));
   if(binderr == -1){
      perror("binding failed");
      exit(-1);
   }

   int listenerr = listen(fd, BACKLOG);
   if(listenerr == -1){
      perror("listening failed");
      exit(-1);
   }

   connect_to_clients(fd);

   close(fd);
}

void connect_to_clients(int listen_fd){
   int terminate = 0;
   while(!terminate){
      struct sockaddr_in client_addr;

      socklen_t addr_len = sizeof(client_addr);
      int client_sock = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
      printf("client connection\n");
      if(client_sock == -1){
         perror("accept failed");
         exit(-1);
      }

      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

      terminate = talk_to_client(client_sock);
      if(terminate){
         printf("server disconnect\n");
      }else{
         printf("client disconnect\n");
      }


      int closeerr = close(client_sock);
      if(closeerr == -1){
         perror("close failed");
      }
   }
}

int talk_to_client(int client_sock){
      crypto_key_t server_keys;
      while(1){
         crypto_msg_t *msg = malloc(MAX_MSG_SIZE);
         size_t n = recv(client_sock, (void*)msg, MAX_MSG_SIZE, 0);
         print_msg_info(msg,server_keys,SERVER_MODE);

         if(n==0){
            return 0;
         }

         int type = msg->header.msg_type;
         size_t full_size;
         crypto_msg_t *ret;
         switch(type){
            case MSG_CMD_SERVER_STOP:
               return 1;
            case MSG_CMD_CLIENT_STOP:
               return 0;
            case MSG_KEY_EXCHANGE:{
                  crypto_key_t client_keys;
                  gen_key_pair(&server_keys, &client_keys);

                  full_size = sizeof(crypto_msg_t)+sizeof(crypto_key_t);
                  ret = malloc(full_size);

                  ret->header.payload_len = sizeof(crypto_key_t);
                  ret->header.direction = DIR_RESPONSE;
                  ret->header.msg_type = MSG_KEY_EXCHANGE;

                  memcpy(&ret->payload, &client_keys, sizeof(crypto_key_t));

                  break;
               }
            case MSG_ENCRYPTED_DATA:{
                  char prefix[] = "echo ";
                  size_t new_size = msg->header.payload_len+strlen(prefix);
                  full_size = sizeof(crypto_msg_t) + new_size;

                  decrypt_string(server_keys, msg->payload, msg->payload, msg->header.payload_len);
                  
                  ret = malloc(full_size);
                  strcpy(ret->payload,prefix);
                  strcat(ret->payload,msg->payload);

                  ret->header.payload_len = new_size;
                  ret->header.direction = DIR_RESPONSE;
                  ret->header.msg_type = MSG_ENCRYPTED_DATA;

                  encrypt_string(server_keys, ret->payload, ret->payload, ret->header.payload_len);

                  char* enc_bytes = calloc(new_size,1);

                  break;
               }
            case MSG_DATA: {
                  char prefix[] = "echo ";
                  size_t new_size = msg->header.payload_len+strlen(prefix);
                  full_size = sizeof(crypto_msg_t) + new_size;

                  ret = malloc(full_size);
                  strcpy(ret->payload,prefix);
                  strcat(ret->payload,msg->payload);

                  ret->header.payload_len = new_size;
                  ret->header.direction = DIR_RESPONSE;
                  ret->header.msg_type = MSG_DATA;

                  break;
               }
         }
         print_msg_info(ret,server_keys,SERVER_MODE);
         send(client_sock, (void *)ret, full_size, 0);
      }
}


