#include "http.h"

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define  BUFF_SZ            1024
#define  MAX_REOPEN_TRIES   5

char recv_buff[BUFF_SZ];

char *generate_cc_request(const char *host, int port, const char *path) {
	static char req[512] = {0};
	int offset = 0;
	
    //note that all paths should start with "/" when passed in
	offset += sprintf((char *)(req + offset),"GET %s HTTP/1.1\r\n", path);
	offset += sprintf((char *)(req + offset),"Host: %s\r\n", host);
	offset += sprintf((char *)(req + offset),"Connection: Keep-Alive\r\n");
	offset += sprintf((char *)(req + offset),"\r\n");

	printf("DEBUG: %s", req);
	return req;
}


void print_usage(char *exe_name) {
    fprintf(stderr, "Usage: %s <hostname> <port> <path...>\n", exe_name);
    fprintf(stderr, "Using default host %s, port %d  and path [\\]\n", DEFAULT_HOST, DEFAULT_PORT); 
}

int reopen_socket(const char *host, uint16_t port) {
    int sock = 0;

    for (int i = 0; i < MAX_REOPEN_TRIES; i++) {
        sock = socket_connect(host, port);
        if (sock < 0) {
            continue;
        } else {
            return sock;
        }
    }
    
    return -1;
}

int server_connect(const char *host, uint16_t port) {
    return socket_connect(host, port);
}

void server_disconnect(int sock){
    close(sock);
}

int submit_request(int sock, const char *host, uint16_t port, char *resource) {
    int sent_bytes = 0; 

    const char *req = generate_cc_request(host, port, resource);
    int send_sz = strlen(req);

    sent_bytes = send(sock, req, send_sz, 0);

    if (sent_bytes < 0) {
        sock = reopen_socket(host, port);
        if (sock < 0) {
            printf("here\n");
            return sock;
        }

        sent_bytes = send(sock, req, send_sz, 0);
    }

    if (sent_bytes != send_sz) {
        if (sent_bytes < 0)
            perror("send failed after reconnect attempt");
        else
            fprintf(stderr, "Sent bytes %d is not equal to sent size %d\n", sent_bytes, send_sz);
        
        close(sock);
        return -1;
    }

    int bytes_recvd = 0; 
    int total_bytes = 0;
    
    //do the first recv
    bytes_recvd = recv(sock, recv_buff, sizeof(recv_buff), 0);
    if (bytes_recvd < 0) {
        perror("initial receive failed");
        close(sock);
        return -1;
    }

    int header_len, content_len;

    int ok_header = process_http_header(recv_buff, bytes_recvd, &header_len, &content_len);
    if (header_len == 0 || ok_header < 0) {
        close(sock);
        return -1;
    }

    //--------------------------------------------------------------------------------
    // TODO:  Make sure you understand the calculations below
    //
    // You do not have to write any code, but add to this comment your thoughts on 
    // what the following 2 lines of code do to track the amount of data received
    // from the server
    //
    // CALCULATION: int initial_data =  bytes_recvd - header_len;
    //              Here we are finding how many bytes of data we snagged in the chunk
    //              with our header data. To find this we are taking the total bytes
    //              received (header + maybe other stuff of the payload) and subtracting
    //              just the length of the header which we calculate in a sub-function call
    //              inside process_http_header(). This leaves us with just the amount of data
    //              after the header AKA 'inital data'.
    //
    // CALCULATION: int bytes_remaining = content_len - initial_data;
    //              Here we are finding how many bytes are remaining in the Content-Length header field.
    //              Since we know the Content-Length specifies a value, say X, but we maybe got some fractional
    //              portion of X in the initial chunk grab, we need to account for this and subtract this
    //              value while reading data from subsequent chunks (i.e. X - initial data, where X is content_len
    //              read in from the header). This prevents us from over reading and looking for data we already have.
    //
    //--------------------------------------------------------------------------------
    int initial_data =  bytes_recvd - header_len;
    int bytes_remaining = content_len - initial_data;

    if (initial_data > 0) {
        fprintf(stdout, "%.*s", (int)initial_data, recv_buff + header_len);
        total_bytes += initial_data;
    }

    while (bytes_remaining > 0) {

        bytes_recvd = recv(sock, recv_buff, bytes_recvd, 0);
        if (bytes_recvd < 0) {
            printf("no socket\n");
            close(sock);
            return -1;
        }

        fprintf(stdout, "%.*s", bytes_recvd, recv_buff);
        total_bytes += bytes_recvd;
        // fprintf(stdout, "remaining %d, received %d\n", bytes_remaining, bytes_recvd);
        bytes_remaining -= bytes_recvd;
    }

    fprintf(stdout, "\n\nOK\n");
    fprintf(stdout, "TOTAL BYTES: %d\n", total_bytes);

    //---------------------------------------------------------------------------------
    // TODO:  Documentation
    //
    // You dont have any code to change, but explain why this function, if it gets to this
    // point returns an active socket.
    //
    // YOUR ANSWER: in this function, if we get to this point, we return an active socket to 
    //              ensure that we and continue to maintain the same connection we have with the
    //              server across multiple requests. In the following code:
    //
    //                  for (int i = 0; i < remaining_args; i++) {
    //                      resource = argv[3+i];
    //                      fprintf(stdout, "\n\nProcessing request for %s\n\n", resource);
    //                      sock = submit_request(sock, host, port, resource);
    //                  }
    //
    //              we see that we are redefining the 'sock' variable so that we can pass the most recently defined
    //              and active socket into the call submit_request(sock, host, port, resource);. 
    //
    //--------------------------------------------------------------------------------
    return sock;
}

int real_main(int argc, char *argv[]) {
    int sock;

    const char *host = DEFAULT_HOST;
    uint16_t   port = DEFAULT_PORT;
    char       *resource = DEFAULT_PATH;
    int        remaining_args = 0;

    //YOU DONT NEED TO DO ANYTHING OR MODIFY ANYTHING IN MAIN().  MAKE SURE YOU UNDERSTAND
    //THE CODE HOWEVER
    sock = server_connect(host, port);

    if (argc < 4) {
        print_usage(argv[0]);
        //process the default request
        submit_request(sock, host, port, resource);
	} else {
        host = argv[1];
        port = atoi(argv[2]);
        resource = argv[3];
        if (port == 0) {
            fprintf(stderr, "NOTE: <port> must be an integer, using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
        fprintf(stdout, "Running with host = %s, port = %d\n", host, port);
        remaining_args = argc-3;
        for (int i = 0; i < remaining_args; i++) {
            resource = argv[3+i];
            fprintf(stdout, "\n\nProcessing request for %s\n\n", resource);
            sock = submit_request(sock, host, port, resource);
        }
    }

    server_disconnect(sock);
}

static void afterRun(int exitCode, const struct timespec *t0, const struct timespec *t1)
{
    double ms = (t1->tv_sec - t0->tv_sec) * 1000.0 + (t1->tv_nsec - t0->tv_nsec) / 1e6;

    fprintf(stderr, "\n--- Run Time ---\n");
    fprintf(stderr, "Elapsed: %.3f ms\n", ms);
}

int main(int argc, char *argv[]) {
    struct timespec t0, t1;
    timespec_get(&t0, TIME_UTC);
    int rc = real_main(argc, argv);
    timespec_get(&t1, TIME_UTC);
    afterRun(rc, &t0, &t1);
    return rc;
}