Ethan Weggel
erw74

Program Objective:

    This program's purpose is to demonstrate the NTP algorithm and protocol by querying an NTP server and calculating round-trip delay, clock offset and dispersion all the while communicating a relative time error for credibility purposes. This program uses pool.ntp.org as a default NTP time server, but can be changed using the '-s' flag and using a different server address.

My Approach:

    My approach for building this NTP client was to build from the ground up and keep things as modular as possible. While using the provided code, especially the header file, I made sure to following the prescribed implementation order so I could build off of what I had already made. This meant I started with my time conversion functions, then moved to my network byte order functions, then my packet building and decoding and printing functions.

    Initially, in order to test my smaller functions, I temporarily added a new flag '-e' to the program for 'experimentation mode'. This is where I could instantiate stack variables and test out functions like getting the current time and populating an ntp timestamp or converting that time using supporting functions. I found that this was paramount to my success as it caught some minor conceptual errors and implementation bugs.

Challenges Faced:

    - In get_current_ntp_time(), I found an error that threw me for a loop. Initially when I performed my calculations I thought all was well until I tried to print my ntp timestamp and saw my milisecond values were consistently zero'd out. I figured this was beyond a rounding issue and looked closer at my code. When I investigated one of the aliased constants reference in the header file, I saw the value was fairly large and that I might be having some overflow issues mid calculation. After modifying data types from uint32_t to uint64_t, just for calculations, I saw this was no longer an issue. Here's the code now:

    void get_current_ntp_time(ntp_timestamp_t *ntp_ts){
        memset(ntp_ts, 0, sizeof(ntp_timestamp_t));

        struct timeval tv;
        if (gettimeofday(&tv, NULL) == -1) {
            perror("Error during gettimeofday()");
        }

        uint64_t modified_seconds = (uint64_t)tv.tv_sec;
        uint64_t modified_useconds = (uint64_t)tv.tv_usec;

        modified_seconds += NTP_EPOCH_OFFSET;
        modified_useconds *= NTP_FRACTION_SCALE;
        modified_useconds /= USEC_INCREMENTS;

        ntp_ts->seconds = modified_seconds;
        ntp_ts->fraction = modified_useconds;

    }

    - When initially looking at the data structure for ntp_packet_t, I was curious why there was no field for the equivalent of T4, until I realized that this could be made entirely separate on the client side. We simply just need to populate a new ntp_timestamp. No big deal, just was a minor hiccup in my implementation process.

    - The last function (print_ntp_results()) suggested we use these buffers: char svr_time_buff[TIME_BUFF_SIZE]; char cli_time_buff[TIME_BUFF_SIZE]; but I had no use for these. I simply called this function I wrote:

    void print_ntp_time(const ntp_timestamp_t *ts, const char* label, int local) {
        char buff[64];
        printf("%s: ", label);
        ntp_time_to_string(ts, buff, sizeof(buff), local);
        puts(buff);
    }

    which already makes a buffer on the stack temporarily to print and then discard between calls. The biggest difference is that I used a different constant (64 instead of the prescribed 32) so I can print whether the time was local or UTC in ntp_time_to_string(). I'm not sure why I needed these buffers unless the intention was to use ntp_time_to_string() in some way which would make more sense. But seeing as though we are already asked to implement print_ntp_time(), I'm not sure why this was suggested. Maybe its a preference thing, not sure, but it doesn't affect functionality.

Example Output:

    Querying NTP server: pool.ntp.org
    Server IP: 158.51.99.19
    Connecting to pool.ntp.org (158.51.99.19) on port 123
    DEBUG: li_vn_mode byte = 0xE3
    Leap Indicator = 3
    Version = 4
    Mode = 3
    Binary breakdown: LI=11 VN=100 Mode=011

    Sending NTP request...
    --- Request Packet ---
    Leap Indicator: 3
    Version: 4
    Mode: 3
    Stratum: 0
    Poll: 6
    Precision: -20
    Reference ID: [0x00000000] NONE
    Root Delay: 0
    Root Dispersion: 0
    Reference Time: 1899-12-31 19:00:00.000000 (Local Time)
    Original Time: 1899-12-31 19:00:00.000000 (Local Time)
    Receive Time: 1899-12-31 19:00:00.000000 (Local Time)
    Transmit Time: 2025-10-15 15:36:02.880669 (Local Time)

    Received NTP response from pool.ntp.org!
    --- Response Packet ---
    Leap Indicator: 0
    Version: 4
    Mode: 4
    Stratum: 2
    Poll: 6
    Precision: -25
    Reference ID: [0xCC093677] 204.9.54.119
    Root Delay: 111
    Root Dispersion: 64
    Reference Time: 2025-10-15 15:21:21.114807 (Local Time)
    Original Time: 2025-10-15 15:36:02.880669 (Local Time)
    Receive Time: 2025-10-15 15:36:02.891395 (Local Time)
    Transmit Time: 2025-10-15 15:36:02.891415 (Local Time)

    === NTP Time Synchronization Results ===
    Server: pool.ntp.org
    Server Time: 2025-10-15 15:36:02.891415 (Local Time)
    Client Time: 2025-10-15 15:36:02.901620 (Local Time)
    Round Trip Delay: 0.020931

    Time Offset: 0.000260 seconds
    Final Dispersion: 0.011442

    Your clock is running BEHIND by 0.259876ms
    Your estimated time error will be +/- 11.442199ms