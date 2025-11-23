#pragma once

#define PROG_MD_CLI     0
#define PROG_MD_SVR     1
#define DEF_PORT_NO     2080
#define FNAME_SZ        150
#define PROG_DEF_FNAME  ""
#define PROG_DEF_SVR_ADDR   "127.0.0.1"

#define MSG_FILE_REQUEST    10
#define MSG_FILE_OK         20
#define MSG_FILE_ERR        30
#define MSG_DATA            40
#define MSG_DATA_OK         50
#define MSG_DATA_END        60
#define MSG_ERROR           70
#define MSG_CLOSE           80

typedef struct prog_config{
    int     prog_mode;
    int     port_number;
    char    svr_ip_addr[16];
    char    file_name[128];
} prog_config;

typedef struct ftp_pdu {
    int         msg_type;
    long        file_size;
    char        file_name[128];
    int         byte_number;
    int         payload_size;
} ftp_pdu;