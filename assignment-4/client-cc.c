#include "http.h"

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define  BUFF_SZ 1024

char recv_buff[BUFF_SZ];

char *generate_cc_request(const char *host, int port, const char *path) {
	static char req[512] = {0};
	int offset = 0;
	
    //note that all paths should start with "/" when passed in
	offset += sprintf((char *)(req + offset),"GET %s HTTP/1.1\r\n", path);
	offset += sprintf((char *)(req + offset),"Host: %s\r\n", host);
	offset += sprintf((char *)(req + offset),"Connection: Close\r\n");
	offset += sprintf((char *)(req + offset),"\r\n");

	printf("DEBUG: %s", req);
	return req;
}


void print_usage(char *exe_name) {
    fprintf(stderr, "Usage: %s <hostname> <port> <path...>\n", exe_name);
    fprintf(stderr, "Using default host %s, port %d  and path [\\]\n", DEFAULT_HOST, DEFAULT_PORT); 
}

int process_request(const char *host, uint16_t port, char *resource) {
    int sock;
    int total_bytes = 0;

    int current_header_sz = 0;
    char* header_buff = (char*)malloc(current_header_sz);

    // make new socket connection
    sock = socket_connect(host, port);
    if (sock < 0) {
        return sock;
    }

    // make new request
    char* req = generate_cc_request(host, port, resource);

    // send request + error check
    int bytes_sent = send(sock, req, strlen(req), 0);
    if (bytes_sent < 0) {
        printf("[ERROR] Could not send request to server...\n");
        return -1;
    }

    // define accumulator + loop
    int bytes_received = 0;
    int header_len, remaining_content_len;
    int header_complete = 0;
    int header_bytes = 0;
    while (1) {
        // receive 1024 chunk
        bytes_received = recv(sock, recv_buff, BUFF_SZ, 0);
        if (bytes_received <= 0) {
            break;
        }

        if (!header_complete) {
            
            if (header_bytes + bytes_received > current_header_sz) {
                int new_sz = current_header_sz ? current_header_sz + 1024 : 1024;
                while (new_sz < header_bytes + bytes_received) {
                    new_sz += 1024;
                }

                char *tmp = (char*)realloc(header_buff, new_sz);
                if (!tmp) { 
                    printf("[ERROR] Problem allocating memory for header. Quitting...\n");
                    break;
                }
                header_buff = tmp;
                current_header_sz = new_sz;
            }

            memcpy(header_buff + header_bytes, recv_buff, bytes_received);
            header_bytes += bytes_received;

            int ok = process_http_header(header_buff, header_bytes, &header_len, &remaining_content_len);
            if (ok == 0) {
                header_complete = 1;

                print_header(header_buff, header_len);

                int body_bytes_after_header = header_bytes - header_len;
                if (body_bytes_after_header > 0) {
                    //print part of body after header
                    printf("%.*s", body_bytes_after_header, header_buff + header_len);
                    
                    if (remaining_content_len >= 0) {
                        remaining_content_len -= body_bytes_after_header;
                        if (remaining_content_len <= 0) {
                            break;
                        }
                    }
                }

            } else {
                continue;
            }

        } else {
            // we are past the header now
            if (remaining_content_len > 0) {
                printf("%.*s", bytes_received, recv_buff);
                remaining_content_len -= bytes_received;
                if (remaining_content_len == 0) { 
                    printf("\n"); 
                    break; 
                }
            } else {
                printf("%.*s\n", bytes_received, recv_buff);
                break;
            }
        }

        total_bytes += bytes_received;
    }

    close(sock);
    free(header_buff);
    return total_bytes;
}

int real_main(int argc, char *argv[]) {
    int sock;

    const char *host = DEFAULT_HOST;
    uint16_t   port = DEFAULT_PORT;
    char       *resource = DEFAULT_PATH;
    int        remaining_args = 0;

    // Command line argument processing should be all setup, you should not need
    // to modify this code
    if(argc < 4) {
        print_usage(argv[0]);
        //process the default request
        process_request(host, port, resource);
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
            process_request(host, port, resource);
        }
    }
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