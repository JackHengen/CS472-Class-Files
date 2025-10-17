/*
 * NTP Client - Network Programming Assignment
 * 
 * This program implements a simple NTP (Network Time Protocol) client that:
 * 1. Connects to an NTP server (default: pool.ntp.org)
 * 2. Sends a time synchronization request
 * 3. Processes the server's response
 * 4. Calculates time offset and network delay
 * 
 * LEARNING OBJECTIVES:
 * - Understanding binary protocol data units (PDUs)
 * - Working with packed C structures for network protocols
 * - Handling network byte order (htonl/ntohl)
 * - Time representation and conversion
 * - Basic network time synchronization concepts
 * 
 * COMPILE: make
 * RUN:     ./ntp-client
 *          ./ntp-client -s time.nist.gov
 * 
 * STUDENT INSTRUCTIONS:
 * Complete all functions marked with "STUDENT TODo" below.
 * Follow the implementation order suggested in the header file.
 * Refer to the detailed comments for guidance on each function.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include "ntp-protocol.h"

void tests();

// Default NTP servers - you can test with different ones!
#define DEFAULT_NTP_SERVER "pool.ntp.org"
#define TIMEOUT_SECONDS 5

/*
 * =============================================================================
 * PROVIDED FUNCTIONS - NETWORKING AND PROGRAM STRUCTURE
 * These functions handle the network communication and program flow.
 * Students should NOT modify these functions.
 * =============================================================================
 */

// Main function - handles command line arguments and starts the NTP query
int main(int argc, char* argv[]) {
    char* ntp_server = DEFAULT_NTP_SERVER;
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "s:hdt")) != -1) {
        switch (opt) {
            case 's':
                ntp_server = optarg;
                break;
            case 'd':
                // Debug mode - demonstrate epoch conversion
                printf("=== DEBUG MODE ===\n");
                demonstrate_epoch_conversion();
                printf("\n");
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            case 't':
                tests();
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    
    printf("Querying NTP server: %s\n", ntp_server);
    
    // Resolve hostname to IP address
    char server_ip[INET_ADDRSTRLEN];
    if (resolve_hostname(ntp_server, server_ip) < 0) {
        fprintf(stderr, "Failed to resolve hostname: %s\n", ntp_server);
        return 1;
    }
    
    printf("Server IP: %s\n", server_ip);
    
    // Query the NTP server
    int result = query_ntp_server(ntp_server, server_ip);
    return result;
}

// Print usage information
void usage(const char* progname) {
    printf("Usage: %s [-s server] [-d] [-h]\n", progname);
    printf("\nOptions:\n");
    printf("  -s server    NTP server to query (default: %s)\n", DEFAULT_NTP_SERVER);
    printf("  -d           Debug mode - show epoch conversion example\n");
    printf("  -h           Show this help\n");
    printf("\nExamples:\n");
    printf("  %s\n", progname);
    printf("  %s -s time.nist.gov\n", progname);
    printf("  %s -s pool.ntp.org\n", progname);
    printf("  %s -d\n", progname);
}

// Resolve hostname to IP address using DNS
int resolve_hostname(const char* hostname, char* ip_str) {
    struct hostent* host_entry = gethostbyname(hostname);
    if (host_entry == NULL) {
        return -1;
    }
    
    struct in_addr addr;
    memcpy(&addr, host_entry->h_addr_list[0], sizeof(struct in_addr));
    strcpy(ip_str, inet_ntoa(addr));
    
    return 0;
}

// Create UDP socket with appropriate timeout settings
int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    // Set timeout for receive operations
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Send NTP request packet over UDP
int send_ntp_request(int sockfd, const struct sockaddr_in* server_addr, 
                     const ntp_packet_t* packet) {
    ssize_t sent = sendto(sockfd, packet, sizeof(ntp_packet_t), 0,
                         (struct sockaddr*)server_addr, sizeof(struct sockaddr_in));
    
    if (sent != sizeof(ntp_packet_t)) {
        perror("sendto");
        return -1;
    }
    
    return 0;
}

// Receive NTP response packet over UDP
int recv_ntp_response(int sockfd, ntp_packet_t* packet) {
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(sockfd, packet, sizeof(ntp_packet_t), 0,
                               (struct sockaddr*)&from_addr, &from_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "NTP request timed out\n");
        } else {
            perror("recvfrom");
        }
        return -1;
    }
    
    if (received != sizeof(ntp_packet_t)) {
        fprintf(stderr, "Received incomplete NTP packet: %zd bytes\n", received);
        return -1;
    }
    
    return 0;
}

// Main NTP query function - coordinates the entire NTP exchange
// This function orchestrates the complete NTP protocol exchange
int query_ntp_server(const char* server_name, const char* ip_str) {
    // Create UDP socket
    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        return -1;
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NTP_PORT);
    if (inet_pton(AF_INET, ip_str, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ip_str);
        close(sockfd);
        return -1;
    }
    
    printf("Connecting to %s (%s) on port %d\n", server_name, ip_str, NTP_PORT);
    
    // Build NTP request packet
    ntp_packet_t request_packet;
    if (build_ntp_request(&request_packet) < 0) {
        fprintf(stderr, "Failed to build NTP request\n");
        close(sockfd);
        return -1;
    }
    
    printf("\nSending NTP request...\n");
    print_ntp_packet_info(&request_packet, "Request", IS_REQUEST);
    
    // Convert to network byte order first, then send
    ntp_to_net(&request_packet);    
    if (send_ntp_request(sockfd, &server_addr, &request_packet) < 0) {
        fprintf(stderr, "Failed to send NTP request\n");
        close(sockfd);
        return -1;
    }
    
    // Receive NTP response
    ntp_packet_t response_packet;
    if (recv_ntp_response(sockfd, &response_packet) < 0) {
        fprintf(stderr, "Failed to receive NTP response\n");
        close(sockfd);
        return -1;
    }

    // FIRST thing to do is get receive time (T4) for accurate timing
    ntp_timestamp_t recv_time;
    get_current_ntp_time(&recv_time);
    
    // Convert both packets back to host byte order for processing
    ntp_to_host(&request_packet);
    ntp_to_host(&response_packet);

    printf("\nReceived NTP response from %s!\n", server_name);
    print_ntp_packet_info(&response_packet, "Response", IS_RESPONSE);
    
    // Calculate time offset and delay using NTP algorithm
    ntp_result_t result;
    if (calculate_ntp_offset(&request_packet, &response_packet, &recv_time, &result) < 0) {
        fprintf(stderr, "Failed to calculate time offset\n");
        close(sockfd);
        return -1;
    }
    
    printf("\n=== NTP Time Synchronization Results ===\n");
    printf("Server: %s\n", server_name);
    print_ntp_results(&result);
    
    close(sockfd);
    return 0;
}

/*
 * =============================================================================
 * DEBUGGING HELPER FUNCTIONS - PROVIDED FOR STUDENT USE
 * =============================================================================
 */

// Debug function to show bit field breakdown
void debug_print_bit_fields(const ntp_packet_t* packet) {
    uint8_t li = GET_NTP_LI(packet);
    uint8_t vn = GET_NTP_VN(packet);  
    uint8_t mode = GET_NTP_MODE(packet);
    
    printf("DEBUG: li_vn_mode byte = 0x%02X\n", packet->li_vn_mode);
    printf("  Leap Indicator = %d\n", li);
    printf("  Version = %d\n", vn);
    printf("  Mode = %d\n", mode);
    printf("  Binary breakdown: LI=%d%d VN=%d%d%d Mode=%d%d%d\n",
           (li >> 1) & 1, li & 1,
           (vn >> 2) & 1, (vn >> 1) & 1, vn & 1,
           (mode >> 2) & 1, (mode >> 1) & 1, mode & 1);
}

// Demonstration function for epoch conversion
void demonstrate_epoch_conversion(void) {
    // Get current time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    uint32_t unix_seconds = tv.tv_sec;
    uint32_t ntp_seconds = unix_seconds + NTP_EPOCH_OFFSET;
    
    printf("=== EPOCH CONVERSION EXAMPLE ===\n");
    printf("Current Unix time: %u seconds since 1970\n", unix_seconds);
    printf("Same time in NTP:  %u seconds since 1900\n", ntp_seconds);
    printf("Difference:        %u seconds (70 years)\n", (uint32_t)NTP_EPOCH_OFFSET);
    printf("Human readable:    %s", ctime((time_t*)&tv.tv_sec));
    printf("Valid NTP range:   ~3.9 billion seconds (for 2025)\n");
    printf("Valid Unix range:  ~1.7 billion seconds (for 2025)\n");
}

/*
 * =============================================================================
 * STUDENT IMPLEMENTATION SECTION
 * Complete the functions below according to the specifications.
 * Follow the implementation order suggested in the header file.
 * =============================================================================
 */

/*
 * GROUP 1: TIME CONVERSION FUNCTIONS
 * These functions handle time format conversions between system time and NTP time.
 */

//STUDENT DONE
/*
 * Get current system time and convert to NTP timestamp format
 * 
 * WHAT TO DO:
 * 1. Use gettimeofday() to get current Unix time (seconds + microseconds)
 * 2. Convert Unix epoch (1970) to NTP epoch (1900) by adding NTP_EPOCH_OFFSET
 * 3. Convert microseconds to NTP fractional format (1/2^32 units)
 * 
 * KEY C FUNCTIONS TO USE:
 * - gettimeofday() - gets current time as seconds + microseconds
 * 
 * EXAMPLE BEHAVIOR:
 * If current time is Sept 15, 2025 13:36:14.541216 UTC:
 * - ntp_ts->seconds should be ~3933894574 (includes NTP_EPOCH_OFFSET)
 * - ntp_ts->fraction should be ~2324671300 (541216 microseconds converted)
 * 
 * MATH HINT:
 * To convert microseconds to NTP fraction: (microseconds * 2^32) / 1,000,000
 * 
 * DEBUGGING TIP:
 * Use demonstrate_epoch_conversion() to verify your conversion logic
 */
void get_current_ntp_time(ntp_timestamp_t *ntp_ts){
    struct timeval tv;
    gettimeofday(&tv,NULL);

    ntp_timestamp_t t;

    // DONE: Implement this function
    // Hint: Use gettimeofday(), convert epoch, scale microseconds
    memset(ntp_ts, 0, sizeof(ntp_timestamp_t));
    ntp_ts->seconds = UNIX_TO_NTP_SECONDS(tv.tv_sec);
    ntp_ts->fraction = MICROSECONDS_TO_FRACTIONS(tv.tv_usec);
}

void current_timestamp_test(){
   ntp_timestamp_t t;
   get_current_ntp_time(&t);

   printf("NTP SECONDS SINCE EPOCH: %u\nNTP FRACTIONS: %u\n",t.seconds,t.fraction);
}
//STUDENT DONE
/*
 * Convert NTP timestamp to human-readable string
 * 
 * WHAT TO DO:
 * 1. Convert NTP timestamp back to Unix time (subtract NTP_EPOCH_OFFSET)
 * 2. Convert NTP fraction back to microseconds
 * 3. Use localtime() or gmtime() based on 'local' parameter
 * 4. Format using snprintf() in "YYYY-MM-DD HH:MM:SS.uuuuuu" format
 * 
 * KEY C FUNCTIONS TO USE:
 * - localtime() - converts to local timezone
 * - gmtime() - converts to UTC
 * - snprintf() - formats the string
 * 
 * EXAMPLE BEHAVIOR:
 * Input: NTP timestamp for Sept 15, 2025 13:36:14.541216
 * Output: "2025-09-15 13:36:14.541216" (if UTC) or local timezone equivalent
 * 
 * MATH HINT:
 * To convert NTP fraction to microseconds: (fraction * 1,000,000) / 2^32
 * 
 * ERROR HANDLING:
 * If conversion fails, use snprintf to write "INVALID_TIME" to buffer
 */
void ntp_time_to_string(const ntp_timestamp_t *ntp_ts, char *buffer, size_t buffer_size, int local) {
    time_t unix_seconds = NTP_TO_UNIX_SECONDS(ntp_ts->seconds);
    time_t microseconds = FRACTIONS_TO_MICROSECONDS(ntp_ts->fraction);
    struct tm *t;
    if(local){
      t = localtime(&unix_seconds);
    } else{
      t = gmtime(&unix_seconds);
    }

    // DONE: Finish the formatting and test
   //t->
    snprintf(buffer, buffer_size, "%d-%d-%d %d:%02d:%02d.%06ld",1900+t->tm_year,1+t->tm_mon,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,microseconds);
}

void ntp_to_string_test(){
   ntp_timestamp_t t;
   get_current_ntp_time(&t);

   size_t alloc_size = 50;
   char *local_str = malloc(alloc_size);
   char *gm_str = malloc(alloc_size);
   ntp_time_to_string(&t,local_str,alloc_size,1);
   ntp_time_to_string(&t,gm_str,alloc_size,0);

   printf("local str: %s\ngm str: %s\n",local_str,gm_str);
}

//STUDENT DONE
/*
 * Convert NTP timestamp to double for mathematical operations
 * 
 * WHAT TO DO:
 * 1. Convert seconds part to double
 * 2. Convert fraction part to decimal fraction (divide by 2^32)
 * 3. Add them together
 * 
 * KEY C FUNCTIONS TO USE:
 * - Standard arithmetic operations
 * - Type casting to double
 * 
 * EXAMPLE BEHAVIOR:
 * Input: seconds=3933894574, fraction=2324671300
 * Output: 3933894574.541216 (approximately)
 * 
 * PURPOSE:
 * This allows precise mathematical operations needed for NTP calculations
 * 
 * PRECISION NOTE:
 * Use NTP_FRACTION_SCALE (2^32) constant for the division
 */
double ntp_time_to_double(const ntp_timestamp_t* timestamp) {
    // DONE: Implement this function
    // Hint: Convert both parts to double and add
    return timestamp->seconds + FRACTIONS_TO_MICROSECONDS(timestamp->fraction)/(double)pow(10,6);
}

void to_double_test(){
   ntp_timestamp_t t;
   get_current_ntp_time(&t);

   double dub = ntp_time_to_double(&t);
   printf("NTP timestamp to double %f\n",dub);
}

//STUDENT DONE
/*
 * Print NTP timestamp with descriptive label
 * 
 * WHAT TO DO:
 * 1. Use ntp_time_to_string() to convert timestamp to string
 * 2. Print with appropriate label and timezone indicator
 * 
 * KEY C FUNCTIONS TO USE:
 * - printf() - for formatted output
 * 
 * EXAMPLE BEHAVIOR:
 * Input: timestamp for current time, label="Transmit Time", local=1
 * Output: "Transmit Time: 2025-09-15 13:36:14.541216 (Local Time)"
 */
void print_ntp_time(const ntp_timestamp_t *ts, const char* label, int local){
    char* suffix = local ? "Local Time" : "GMT Time";
    size_t ntp_str_size = 50;
    char* ntp_str = malloc(ntp_str_size);
    ntp_time_to_string(ts,ntp_str,ntp_str_size,local);
    printf("%s: %s (%s)\n",label,ntp_str,suffix);
    // DONE: Implement this function
    // Hint: Use ntp_time_to_string and printf
}

void print_ntp_test(){
   ntp_timestamp_t t;
   get_current_ntp_time(&t);
   print_ntp_time(&t,"Label Text",1);
}
/*
 * GROUP 2: NETWORK BYTE ORDER FUNCTIONS
 * These functions handle conversion between host and network byte order.
 * Network protocols require big-endian byte order regardless of host architecture.
 */

//STUDENT DONE
/*
 * Convert NTP timestamp from host to network byte order
 * 
 * WHAT TO DO:
 * 1. Convert both seconds and fraction fields using htonl()
 * 2. Modify the structure in place
 * 
 * KEY C FUNCTIONS TO USE:
 * - htonl() - host to network long (32-bit conversion)
 * 
 * EXAMPLE BEHAVIOR:
 * Input: timestamp with seconds=0x12345678 (little-endian host)
 * Output: timestamp with seconds=0x78563412 (big-endian network)
 * 
 * WHY NEEDED:
 * Network protocols require consistent byte order across different architectures
 */
void ntp_ts_to_net(ntp_timestamp_t* timestamp){
    timestamp->seconds = htonl(timestamp->seconds);
    timestamp->fraction = htonl(timestamp->fraction);
    // DONE: Implement this function
    // Hint: Use htonl() on both seconds and fraction fields
}

//DISCLAIMER - THIS IS COPIED FROM CHATGPT AND THEN SLIGHTLY MODIFIED THIS IS NOT MY ORIGINAL CODE
void print_bits(const void *ptr, size_t size) {
    const unsigned char *b = ptr;
    for (size_t i = 0; i < size; i++) {
        for (int j = 7; j >= 0; j--) {   // MSB first
            putchar((b[i] & (1 << j)) ? '1' : '0');
        }
        putchar(' ');  // optional separator between bytes
    }
    putchar('\n');
}

//STUDENT DONE
/*
 * Convert NTP timestamp from network to host byte order
 * 
 * WHAT TO DO:
 * 1. Convert both seconds and fraction fields using ntohl()
 * 2. Modify the structure in place
 * 
 * KEY C FUNCTIONS TO USE:
 * - ntohl() - network to host long (32-bit conversion)
 * 
 * EXAMPLE BEHAVIOR:
 * Input: timestamp with seconds=0x78563412 (big-endian network)
 * Output: timestamp with seconds=0x12345678 (little-endian host)
 * 
 * WHY NEEDED:
 * Host processing requires native byte order for correct arithmetic
 */
void ntp_ts_to_host(ntp_timestamp_t* timestamp){
    timestamp->seconds = ntohl(timestamp->seconds);
    timestamp->fraction = ntohl(timestamp->fraction);
    // DONE: Implement this function
    // Hint: Use ntohl() on both seconds and fraction fields
}

void ts_to_host_test(){
   ntp_timestamp_t t;
   get_current_ntp_time(&t);

   printf("ts_to_host_test()\n");

   printf("NTP SECONDS HOST: ");
   print_bits(&t.seconds,4);
   
   ntp_ts_to_net(&t);

   printf("NTP SECONDS NET: ");
   print_bits(&t.seconds,4);

   ntp_ts_to_host(&t);

   printf("NTP SECONDS HOST AGAIN: ");
   print_bits(&t.seconds,4);

   get_current_ntp_time(&t);

   printf("NTP FRACTIONS HOST: ");
   print_bits(&t.fraction,4);

   ntp_ts_to_host(&t);

   printf("NTP FRACTIONS NET: ");
   print_bits(&t.fraction,4);

   ntp_ts_to_host(&t);

   printf("NTP FRACTIONS HOST AGAIN: ");
   print_bits(&t.fraction,4);
}

//STUDENT DONE
/*
 * Convert entire NTP packet from host to network byte order
 * 
 * WHAT TO DO:
 * 1. Convert all 32-bit fields using htonl(): root_delay, root_dispersion, reference_id
 * 2. Convert all timestamp fields using ntp_ts_to_net()
 * 3. Leave 8-bit fields unchanged (li_vn_mode, stratum, poll, precision)
 * 
 * KEY C FUNCTIONS TO USE:
 * - htonl() - for 32-bit fields
 * - ntp_ts_to_net() - for timestamp fields
 * 
 * EXAMPLE BEHAVIOR:
 * Converts all multi-byte fields in packet from host to network byte order
 * Single-byte fields (stratum, poll, etc.) remain unchanged
 * 
 * CALL THIS: Before sending packet over network
 */
void ntp_to_net(ntp_packet_t* packet){
    packet->root_delay = htonl(packet->root_delay);
    packet->root_dispersion = htonl(packet->root_dispersion);
    packet->reference_id = htonl(packet->reference_id);
    ntp_ts_to_net(&packet->orig_time);
    ntp_ts_to_net(&packet->recv_time);
    ntp_ts_to_net(&packet->ref_time);
    ntp_ts_to_net(&packet->xmit_time);
    // DONE: Implement this function
    // Hint: Convert 32-bit fields with htonl(), timestamps with ntp_ts_to_net()
}

//STUDENT DONE
/*
 * Convert entire NTP packet from network to host byte order
 * 
 * WHAT TO DO:
 * 1. Convert all 32-bit fields using ntohl(): root_delay, root_dispersion, reference_id
 * 2. Convert all timestamp fields using ntp_ts_to_host()
 * 3. Leave 8-bit fields unchanged
 * 
 * KEY C FUNCTIONS TO USE:
 * - ntohl() - for 32-bit fields
 * - ntp_ts_to_host() - for timestamp fields
 * 
 * EXAMPLE BEHAVIOR:
 * Converts all multi-byte fields in packet from network to host byte order
 * 
 * CALL THIS: After receiving packet from network
 */
void ntp_to_host(ntp_packet_t* packet){
    packet->root_delay = ntohl(packet->root_delay);
    packet->root_dispersion = ntohl(packet->root_dispersion);
    packet->reference_id = ntohl(packet->reference_id);
    ntp_ts_to_host(&packet->orig_time);
    ntp_ts_to_host(&packet->recv_time);
    ntp_ts_to_host(&packet->ref_time);
    ntp_ts_to_host(&packet->xmit_time);


    // DONE: Implement this function
    // Hint: Convert 32-bit fields with ntohl(), timestamps with ntp_ts_to_host()
}

void print_ntp_packet(ntp_packet_t* packet){

   char* sep = " | ";

   printf(sep);

   print_bits(&packet->li_vn_mode,1);         
   printf(sep);
   print_bits(&packet->stratum,1);
   printf(sep);
   print_bits(&packet->poll,1);
   printf(sep);
   print_bits(&packet->precision,1);
   printf(sep);

   print_bits(&packet->root_delay,4);
   printf(sep);
   print_bits(&packet->root_dispersion,4);
   printf(sep);
   print_bits(&packet->reference_id,4);
   printf(sep);

   print_bits(&packet->ref_time,4);
   printf(sep);
   print_bits(&packet->orig_time,4);
   printf(sep);
   print_bits(&packet->recv_time,4);
   printf(sep);
   print_bits(&packet->xmit_time,4);
   printf(sep);
}


/*
 * GROUP 3: NTP PACKET CONSTRUCTION
 * These functions build NTP protocol packets according to RFC specifications.
 */

//STUDENT DONE
/*
 * Build NTP client request packet
 * 
 * WHAT TO DO:
 * 1. Clear entire packet with memset()
 * 2. Set leap indicator, version, mode using SET_NTP_LI_VN_MODE macro
 *    - LI: NTP_LI_UNSYNC (3) - client clock not synchronized
 *    - VN: NTP_VERSION (4) - NTP version 4
 *    - Mode: NTP_MODE_CLIENT (3) - client request
 * 3. Set stratum to 0 (unspecified for client)
 * 4. Set poll to 6 (64 second interval)
 * 5. Set precision to -20 (~1 microsecond)
 * 6. Set root_delay and root_dispersion to 0
 * 7. Set reference_id to 0
 * 8. Clear all timestamp fields except transmit time
 * 9. Set transmit time to current time using get_current_ntp_time()
 * 
 * KEY C FUNCTIONS TO USE:
 * - memset() - clear packet structure
 * - memcpy() or assignment - set fields
 * - get_current_ntp_time() - set transmit timestamp
 * 
 * EXAMPLE BEHAVIOR:
 * Creates a valid NTP client request with:
 * - All control fields properly set
 * - Only transmit timestamp filled (others are zero)
 * - Packet ready to send to server
 * 
 * DEBUGGING TIP:
 * Use debug_print_bit_fields() to verify your bit field settings
 * 
 * RETURN VALUE:
 * RC_OK (0) on success, RC_BAD_PACKET (-1) if packet is NULL
 */
int build_ntp_request(ntp_packet_t* packet) {
    if (!packet) {
        return RC_BAD_PACKET;
    }
    memset(packet, 0, sizeof(ntp_packet_t));

   SET_NTP_LI_VN_MODE(packet,3,4,3);
   packet->poll=6;
   packet->precision=-20;
   // packet->root_dispersion=(1<<16)+2; - testing
   // stratum, root_delay, root_dispersion, reference_id = 0
   get_current_ntp_time(&packet->xmit_time);
    
    return RC_OK;
}

void build_ntp_packet_test(){
   ntp_packet_t p;
   build_ntp_request(&p);

   debug_print_bit_fields(&p); 
}

/*
 * GROUP 4: PROTOCOL ANALYSIS FUNCTIONS
 * These functions parse NTP responses and perform time calculations.
 */

//STUDENT DONE
/*
 * Decode NTP reference_id field based on stratum level
 * 
 * WHAT TO DO:
 * 1. Check buffer size requirements:
 *    - If ref_id == 0: need 5 bytes for "NONE"
 *    - If stratum >= 2: need 16 bytes for IP address string
 *    - If stratum < 2: need 5 bytes for 4-char ASCII code
 * 2. Handle ref_id == 0 case: copy "NONE" to buffer
 * 3. Handle stratum >= 2 case: treat ref_id as IP address
 *    - Convert ref_id to network byte order with htonl()
 *    - Use inet_ntop() to convert to IP string
 * 4. Handle stratum < 2 case: treat ref_id as 4 ASCII characters
 *    - Convert ref_id to network byte order with htonl()
 *    - Copy 4 bytes directly to buffer as characters (like "NIST")
 * 
 * KEY C FUNCTIONS TO USE:
 * - strcpy() - for "NONE" case
 * - htonl() - convert to network byte order for inet_ntop()
 * - inet_ntop() - convert IP address to string
 * - memset() and memcpy() - for ASCII character handling
 * 
 * EXAMPLE BEHAVIOR:
 * - ref_id=0: output="NONE"
 * - stratum=2, ref_id=0xcf424f67: output="207.66.79.103" (IP address)
 * - stratum=1, ref_id=0x4e495354: output="NIST" (ASCII characters)
 * 
 * BUFFER SIZE REQUIREMENTS:
 * - Return RC_BUFF_TOO_SMALL if buffer is too small for any case
 * 
 * RETURN VALUE:
 * RC_OK (0) on success, RC_BUFF_TOO_SMALL (-2) if buffer too small
 */
int decode_reference_id(uint8_t stratum, uint32_t ref_id, char *buff, int buff_sz){
   // DONE: Implement this function
   // Hint: Check buffer sizes, handle ref_id==0, stratum>=2 (IP), stratum<2 (ASCII)
   if(stratum >= 2){
      if(buff_sz < 16){
         return RC_BUFF_TOO_SMALL;
      }
   }else{
      if(buff_sz < 5){
         return RC_BUFF_TOO_SMALL;
      }
   }

   unsigned char *dat = malloc(5);
   dat[0] = ref_id << 3*8 >> 3*8;
   dat[1] = ref_id << 2*8 >> 3*8;
   dat[2] = ref_id << 1*8 >> 3*8;
   dat[3] = ref_id >> 3*8;
   dat[4] = 0;
   if(stratum >=2){
      snprintf(buff,buff_sz,"%u.%u.%u.%u",dat[3],dat[2],dat[1],dat[0]);
   } else{
      if(ref_id==0){
         strcpy(buff,"NONE");
      } else{
         strcpy(buff,dat);
      }
   }

   return RC_OK;
}

void decode_ref_test(){
   size_t s = 16;
   char * buf = malloc(s);
   
   decode_reference_id(1,0,buf,s);
   printf("%s\n",buf);

   decode_reference_id(1,0x474f4f47,buf,s);
   printf("%s\n",buf);

   decode_reference_id(2,0x474f4f47,buf,s);
   printf("%s\n",buf);
}

//STUDENT DONE
/*
 * Calculate NTP time offset and delay using standard algorithm
 * 
 * WHAT TO DO:
 * 1. Extract the four timestamps and convert to double for precise math:
 *    - T1 = request->xmit_time (client request send time)
 *    - T2 = response->recv_time (server receive time)
 *    - T3 = response->xmit_time (server response send time)
 *    - T4 = recv_time (client response receive time)
 * 2. Calculate round-trip delay: delay = (T4 - T1) - (T3 - T2)
 * 3. Calculate time offset: offset = ((T2 - T1) + (T3 - T4)) / 2
 * 4. Calculate final dispersion using server values and computed delay
 * 5. Copy server and client timestamps to result structure
 * 
 * KEY C FUNCTIONS TO USE:
 * - ntp_time_to_double() - convert timestamps for math
 * - GET_NTP_Q1616_TS() - decode server dispersion/delay values
 * - memcpy() - copy timestamp structures
 * 
 * DETAILED MATH EXPLANATION:
 * 
 * NTP DELAY CALCULATION:
 * Formula: delay = (T4 - T1) - (T3 - T2)
 * Meaning: 
 * - (T4 - T1) = total time from client send to client receive
 * - (T3 - T2) = time the packet spent at the server
 * - Subtracting server time gives us pure network transit time
 * - This accounts for server processing delays
 * Example: If total round-trip is 100ms and server held packet for 20ms,
 *          then network delay = 100ms - 20ms = 80ms
 * 
 * NTP OFFSET CALCULATION:
 * Formula: offset = ((T2 - T1) + (T3 - T4)) / 2
 * Meaning:
 * - (T2 - T1) = how much client was behind when sending (includes network delay)
 * - (T3 - T4) = how much client was behind when receiving (includes network delay)
 * - Adding these cancels out network delays in opposite directions
 * - Dividing by 2 gives the average time difference
 * - Positive result: client clock is BEHIND server
 * - Negative result: client clock is AHEAD of server
 * Example: If T2-T1 = +50ms and T3-T4 = +30ms, offset = (50+30)/2 = +40ms behind
 * 
 * FINAL DISPERSION CALCULATION:
 * Formula: final_dispersion = server_dispersion + (server_delay/2) + (delay/2)
 * Meaning:
 * - server_dispersion = server's estimate of its own time accuracy
 * - server_delay/2 = half of server's root delay (accumulated network uncertainty)
 * - delay/2 = half of our measured delay (our network uncertainty)
 * - This gives total estimated error bounds for the time synchronization
 * - Smaller dispersion = more accurate time sync
 * Example: If server dispersion = 5ms, server delay = 10ms, our delay = 20ms
 *          then final_dispersion = 5 + 5 + 10 = 20ms error estimate
 * 
 * WHY THIS WORKS:
 * The NTP algorithm assumes network delays are symmetric (same in both directions).
 * While not always true, this assumption allows us to separate clock offset
 * from network delay using only four timestamps. The math elegantly cancels
 * out the network delay components when calculating offset.
 * 
 * EXAMPLE BEHAVIOR:
 * Input: Four timestamps from NTP exchange
 * Output: Populated ntp_result_t with offset, delay, dispersion, and times
 * 
 * RETURN VALUE:
 * 0 on success, -1 if any pointer is NULL
 */
int calculate_ntp_offset(const ntp_packet_t* request, 
                        const ntp_packet_t* response,
                        const ntp_timestamp_t* recv_time, 
                        ntp_result_t* result) {

    if (!request || !response || !result) {
        return -1;
    }
    
   ntp_timestamp_t *T1= &response->orig_time;
   ntp_timestamp_t *T2= &response->recv_time;
   ntp_timestamp_t *T3= &response->xmit_time;
   ntp_timestamp_t *T4= recv_time;

   double t1 = ntp_time_to_double(T1), t2 = ntp_time_to_double(T2), t3 = ntp_time_to_double(T3), t4 = ntp_time_to_double(T4);
    // Initialize result with dummy values
    memset(&result->server_time, 0, sizeof(ntp_timestamp_t));
    memset(&result->client_time, 0, sizeof(ntp_timestamp_t));
    
    result->delay = (t4-t1)-(t3-t2);
    result->offset = ((t2-t1)-(t3-t4))/2;
    result->final_dispersion = GET_NTP_Q1616_TS(response->root_dispersion) + GET_NTP_Q1616_TS(response->root_delay)/2 + result->delay/2;
    result->client_time = *T4;
    result->server_time = *T3;

    return 0;
}

/*
 * GROUP 5: DISPLAY FUNCTIONS
 * These functions format and display NTP information for the user.
 */

//STUDENT DONE
/*
 * Print detailed NTP packet information in human-readable format
 * 
 * WHAT TO DO:
 * 1. Print packet type header with label
 * 2. Extract and print bit fields using GET_NTP_* macros:
 *    - Leap Indicator, Version, Mode
 * 3. Print basic fields: stratum, poll, precision
 * 4. Decode and print reference_id using decode_reference_id()
 * 5. Print root_delay and root_dispersion values
 * 6. Print all timestamps using print_ntp_time()
 * 
 * KEY C FUNCTIONS TO USE:
 * - printf() - formatted output
 * - GET_NTP_LI(), GET_NTP_VN(), GET_NTP_MODE() - extract bit fields
 * - decode_reference_id() - decode reference field
 * - print_ntp_time() - format timestamps
 * 
 * EXAMPLE OUTPUT:
 * --- Response Packet ---
 * Leap Indicator: 0
 * Version: 4
 * Mode: 4
 * Stratum: 2
 * Poll: 6
 * Precision: -24
 * Reference ID: [0x179b2826] 23.155.40.38
 * Root Delay: 23
 * Root Dispersion 1669
 * Reference Time: 2025-09-15 08:58:17.614668 (Local Time)
 * Original Time (T1): 2025-09-15 09:09:34.232246 (Local Time)
 * Receive Time (T2): 2025-09-15 09:09:34.348225 (Local Time)
 * Transmit Time (T3): 2025-09-15 09:09:34.348244 (Local Time)
 */
void print_ntp_packet_info(const ntp_packet_t* packet, const char* label, int packet_type) {
   size_t ref_size = 16;
   char *ref_id = malloc(ref_size);
   decode_reference_id(packet->stratum,packet->reference_id,ref_id,ref_size);
    printf("--- %s Packet ---\n", label);
    printf("Leap Indicator: %d\n",GET_NTP_LI(packet));
    printf("Version: %d\n",GET_NTP_VN(packet));
    printf("Mode: %d\n",GET_NTP_MODE(packet));
    printf("Stratum: %d\n",packet->stratum);
    printf("Poll: %d\n",packet->poll);
    printf("Precision %d\n",packet->precision);
    printf("Reference ID: %s\n",ref_id);
    printf("Root Delay: %f\n",GET_NTP_Q1616_TS(packet->root_delay));
    printf("Root Dispersion: %f\n",GET_NTP_Q1616_TS(packet->root_dispersion));
    print_ntp_time(&packet->ref_time,"Reference Time",packet_type);
    print_ntp_time(&packet->orig_time,"Original Time",packet_type);
    print_ntp_time(&packet->recv_time,"Receive Time",packet_type);
    print_ntp_time(&packet->xmit_time,"Transmit Time",packet_type);
}

//STUDENT TODO
/*
 * Print NTP synchronization results with user-friendly analysis
 * 
 * WHAT TO DO:
 * 1. Convert server and client timestamps to readable strings
 * 2. Print server time, local time, and round-trip delay
 * 3. Print time offset and final dispersion
 * 4. Analyze offset to determine if clock is ahead or behind
 * 5. Convert values to milliseconds for easier reading
 *
 * EXAMPLE OUTPUT: 
=== NTP Time Synchronization Results ===
Server: pool.ntp.org
Server Time: 2025-09-15 09:09:34.348244 (local time)
Local Time:  2025-09-15 09:09:34.302108 (local time)
Round Trip Delay: 0.069842

Time Offset: 0.081058 seconds
Final dispersion 0.034921

Your clock is running BEHIND by 81.06ms
Your estimated time error will be +/- 34.92ms
 */
void print_ntp_results(const ntp_result_t* result) {
    /* Here are some local buffers that you should use*/
    char svr_time_buff[TIME_BUFF_SIZE];
    char cli_time_buff[TIME_BUFF_SIZE];


    double client_d = ntp_time_to_double(&result->client_time);
    double serv_d = ntp_time_to_double(&result->server_time);
    
    char* s = malloc(fmax(strlen("BEHIND"),strlen("AHEAD"))+1);
    strcpy(s,"AHEAD");
    if(client_d < serv_d){
      strcpy(s,"BEHIND");
    }

    double est_err = result->final_dispersion*1000;
    double est_offset = result->offset*1000;
   
    print_ntp_time(&result->server_time,"Server Time",1); 
    print_ntp_time(&result->client_time,"Client Time",1);
    printf("Round Trip Delay: %f\n",result->delay);
    printf("\n");
    printf("Time Offset: %f seconds\n",result->offset);
    printf("Final dispersion: %f\n",result->final_dispersion);
    printf("\n");
    printf("Your clock is running %s by %fms\n",s,est_offset);
    printf("Your estimated time error will be +/-%fms\n",est_err);
}

void tests(){
   current_timestamp_test();
   ntp_to_string_test();
   to_double_test();
   print_ntp_test();
   ts_to_host_test();
   build_ntp_packet_test();
   decode_ref_test();
}

