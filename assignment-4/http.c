#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <ctype.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "http.h"

char *strcasestr(const char *s, const char *find) {
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

char *strnstr(const char *s, const char *find, size_t slen) {
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0' || slen-- < 1)
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}


//  DOCUMENTATION - erw74
//  
//  int socket_connection(host, port) is a function that, at its core, takes an ip address and port number and 
//  sets up a socket file descriptor for us that is a part of a connection to an http server. This is done by starting 
//  with a hostent struct which is a number of fields that represents a database entry for a particular host. 
//  This is populated by calling gethostbyname() (provided by netdb.h) and it will query a DNS to resolve a hostname 
//  like "www.google.com" into an ip address or set of ip addresses. The function then uses the zeroeth found ip 
//  address to popular a sockaddr_in struct to provide an ip to connect to via our socket. We then use htons(port) 
//  to convert our little endian 16-byte port number to big endian or network byte order and set that field for 
//  our socket struct as well. We also specify that we want to use IPv4 and then create our socket. After this 
//  we form a connection which means our socket will talk to our remote socket in an ongoing conversation until 
//  our server disconnects or our request is finished in the case of client-cc. connect() here will start the TCP 
//  handshake to addr. We then return the socket file descriptor to the user. It is also worth noting that at any
//  point we make a syscall/library function, if we receive an error we use perror() or herror() to show the last
//  error code but also we return a negative number to the user. In this case we return -2 for name resolution,
//  then -1 for anything socket related.
int socket_connect(const char *host, uint16_t port) {
    struct hostent *hp;                                                                     // struct to hold that ip address from db lookup on DNS
    struct sockaddr_in addr;                                                                // struct to hold socket info
    int sock;                                                                               // where we are going to keep our socket file descriptor

    if((hp = gethostbyname(host)) == NULL){                                                 // DNS lookup for hostname resolution
		herror("gethostbyname");
		return -2;
	}
    
    
	bcopy(hp->h_addr_list[0], &addr.sin_addr, hp->h_length);                                // START mem. copy/population into sockaddr_in struct
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, 0);                                                 // END mem. copy/population into sockaddr_in struct 
	
	if(sock == -1){
		perror("socket");
		return -1;
	}

    if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {         // form our TCP based connection
		perror("connect");
        close(sock);                                                                        // if we error out, close that sock_fd
        return -1;
	}

    return sock;                                                                            // return sock_fd to user
}

//  DOCUMENTATION - erw74
// 
//  int get_http_header_len(http_buff, http_buff_len) is a function that will return the header length
//  of an http response placed at the beginning of a char buffer (http_buff) of length http_buff_len, 
//  whose end is denoted '\r\n\r\n' (HTTP_HEADER_END). It starts with defining a char pointer (end_ptr)
//  that will eventually point to the first occurrence of HTTP_HEADER_END. Then we call strnstr and set
//  the return value of it to end_ptr, giving strnstr our http_buff (our haystack), HTTP_HEADER_END (our needle) 
//  and http_buff_len (the size of the haystack). At every pass in that function, we look at the first character
//  of our needle, then go character by character linearly through our haystack. If we find a character match
//  then we do a string comparison of the remainder of our needle and the string of the same number of characters
//  in our haystack following the initial character match. We do this until we find a full match, then adjust our
//  COPY of our pointer in http_buff or our haystack to point to the beginning of the first occurrence. Then we
//  return this copy (as end_ptr) and subtract the starting pointer to the first index of our http_buff. This
//  provides the number of characters between the start of the array and the first occurrence of the HTTP_HEADER_END
//  so we have to then add the length of HTTP_HEADER_END to truly point to the end of the header (otherwise we'd be
//  four characters short). This final calculation is then returned to the user. If we can't find any matches,
//  we return -1.
int get_http_header_len(char *http_buff, int http_buff_len) {
    char *end_ptr;                                                          // set up end_ptr
    int header_len = 0;                                                     // set up length variable
    end_ptr = strnstr(http_buff,HTTP_HEADER_END,http_buff_len);             // call our needle in haystack algorithm
                                                                            //      that will eventually give a pointer
    if (end_ptr == NULL) {                                                  //      to the first occurrence of our needle
        fprintf(stderr, "Could not find the end of the HTTP header\n");
        return -1;
    }

    header_len = (end_ptr - http_buff) + strlen(HTTP_HEADER_END);           // our offset calculation to show the real length of header

    return header_len;                                                      // return header
}

//  DOCUMENTATION - erw74
//
//  int get_http_content_len(http_buff, http_header_len) is a function that will return to us the number that is associated with
//  the 'Content-Length' field in a given HTTP header line if it is present. If not, it will return 0 and will print
//  an appropriate error message. To start, this function allocates a temporary char buff that can store the maximum
//  size of HTTP header line denoted 'MAX_HEADER_LINE'. We then get pointers to the beginning of the provided http_buff and the end
//  so that we can make a conditional to continue to loop while the beginning pointer ('next_header_line') is less than the value of 
//  'end_header_line'. For every iteration of the loop, we copy characters from 'next_header_line' into 'header_line' until we
//  find the characters "\r\n" which is HTTP_HEADER_EOL or the end of a header line in an HTTP response. Then we use strcasestr
//  to do a case insensitive search by converting our needle and haystack characters to lowercase and loop through our haystack
//  until we find a character that matches the first character of our needle. Then we check that the haystack has at least the remaining
//  number of characters needed to match the remainder of our needle. If we find a match, we return a pointer in haystack to the beginning
//  of the found occurrence, otherwise NULL. If we do not get NULL, then we move our starting pointer value to where our HTTP_HEADER_DELIM
//  lies and then add one to account for the space. Then we can parse the remaining string into atoi() which will convert our ascii
//  string version of a number into an actual integer. We then return this number and find the value of our 'Content-Length' field.
int get_http_content_len(char *http_buff, int http_header_len) {
    char header_line[MAX_HEADER_LINE];                                              // make our temp line buffer

    char *next_header_line = http_buff;                                             // start our temp line at the start of http_buff
    char *end_header_buff = http_buff + http_header_len;                            // find the end of http_buff for our loop condition

    while (next_header_line < end_header_buff) {                                    // until we reach the end of our http_buff, keep looping and updating header_line
        bzero(header_line, sizeof(header_line));                                    // reset all values of header_line to zero so any garbage in that memory doesn't interfere with current read
        sscanf(next_header_line, "%[^\r\n]s", header_line);                         // put every character into header_line from http_buff starting at next_header_line until we reach '\r\n'

        int isCLHeader2 = strcasecmp(header_line, CL_HEADER);                       // this is a good check but we don't use it here
        char *isCLHeader = strcasestr(header_line, CL_HEADER);                      // here we look for a case insensitive match of our CL_HEADER inside of header_line
        if(isCLHeader != NULL) {                                                    // if we have a match, we continue    
            char *header_value_start = strchr(header_line, HTTP_HEADER_DELIM);      // fast-forward our pointer to after our delimiter
            if (header_value_start != NULL) {
                char *header_value = header_value_start + 1;                        // add one more to our pointer to account for the SP char after the HTTP_HEADER_DELIM
                int content_len = atoi(header_value);                               // convert the rest of our header into an integer from ascii characters
                return content_len;                                                 // return the number we read and converted
            }
        }
        next_header_line += strlen(header_line) + strlen(HTTP_HEADER_EOL);          // if we haven't found a match in our current header_line, then we fast-forward our
    }                                                                               //      http_buff reference pointer to after our current header line length plus the 
    fprintf(stderr,"Did not find content length\n");                                //      length of the end of line characters so that our next header_line we copy in
    return 0;                                                                       //      is truly the start of a new line in our http_buff
}                                                                                   // we also return 0 if we found no matches period so the rest of our code can infer we don't have a Content-Length field so the length is 0

void print_header(char *http_buff, int http_header_len) {
    fprintf(stdout, "%.*s\n", http_header_len,http_buff);
}

//--------------------------------------------------------------------------------------
// EXTRA CREDIT - 10 pts - READ BELOW
//
// Implement a function that processes the header in one pass to figure out BOTH the
// header length and the content length.  I provided an implementation below just to 
// highlight what I DONT WANT, in that we are making 2 passes over the buffer to determine
// the header and content length.
//
// To get extra credit, you must process the buffer ONCE getting both the header and content
// length.  Note that you are also free to change the function signature, or use the one I have
// that is passing both of the values back via pointers.  If you change the interface dont forget
// to change the signature in the http.h header file :-).  You also need to update client-ka.c to 
// use this function to get full extra credit. 
//--------------------------------------------------------------------------------------


// PARAMS: 
// http_buff    -> buffer of http response
//         L    -> left window pointer
//         R    -> right window pointer
// 
// RETURN:
//        -1    -> nothing found
//         1    -> found CL_HEADER
//         2    -> found HTTP_HEADER_END
//
// NOTE: 
//         L gets set to the index of the first '\n' character in CL_HEADER in RET 1
//         R get set to the index of the beginning of HTTP_HEADER_END in RET 2 
int contains_object(char* http_buff, int* l, int* r) {
    char win[15];

    for (int i = 0; i < 14; i++) {                                              // copy characters from http_buff into our temp window for evaluation (between l and r inclusive)
        win[i] = http_buff[i+*l];
    }
    win[15] = '\0';

    if (strcmp(CL_HEADER, win) == 0) {                                          // 14 character with a null terminator will fit CL_HEADER exactly if we match
        return 1;
    }

    char* found_ptr = strnstr(win, HTTP_HEADER_END, sizeof(HTTP_HEADER_END));   // otherwise we use needle in haystack algorithm to look for HTTP_HEADER_END in our window
    if (found_ptr != NULL) {
        ptrdiff_t off = found_ptr - win;
        *l += (int)off;
        return 2;
    }
    
    return -1;
}

int process_http_header(char* http_buff, int http_buff_len, int* header_len, int* content_len) {
    // classic sliding window technique where every char ptr looks for a character in CL_HEADER until we find it
    // at every iteration we also look for HTTP_HEADER_END
    if (http_buff_len < 14) {
        *header_len = 0;
        *content_len = 0;
        return -1;
    }

    int contains_obj;
    int l = 0, r = 13;
    int f_CL_HEADER = 0, f_HTTP_HEADER_END = 0;
    while (r < http_buff_len) {                                         // make sure our right index pointer doesn't reach the end of our buffer

        contains_obj = contains_object(http_buff, &l, &r);              // we see if our current characters between l and r (inclusive) contain a CL_HEADER, HTTP_HEADER_END or nothing
        if (contains_obj == -1) {                                       // if we found nothing, no need for extra time-wasting checks, we just advance our window and continue
            l++;
            r++;
            continue;
        }

        // we found CL_HEADER
        if (contains_obj == 1) {
            f_CL_HEADER = 1;
            char dig_str[24];                                           // arbitrarily setting the content-length string to 23 + null term. characters long

            int idx = 0;
            while (idx < 24) {
                if (http_buff[l+strlen(CL_HEADER)+idx] == '\n') {       // if we read all of our digits, null terminate and exit loop
                    dig_str[idx] = '\0';
                    break;
                }

                dig_str[idx] = http_buff[l+strlen(CL_HEADER)+2+idx];    // add new digit to the dig_str array. (+2 is to account for DELIM and SP)
                idx++;
            }

            *content_len = atoi(dig_str);                               // we convert our content length to a number and set the value of the dereferenced pointer
        }
        
        // we found HTTP_HEADER_END
        if (contains_obj == 2) {
            f_HTTP_HEADER_END = 1;
            *header_len = l + strlen(HTTP_HEADER_END);                  // we already know the position of l after this, so we just add the length of the '\r\n\r\n'
        }

        l++;
        r++;

        // in our loop, we expect to find both a CL_HEADER and HTTP_HEADER_END at some point, so we quit when we find both to save time
        if (f_CL_HEADER && f_HTTP_HEADER_END) {
            return 0;
        }
    }

}
