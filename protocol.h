#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <sys/types.h>

#define MAX_PROCESS_NAME 256
#define MAX_ARGS 32
#define MAX_ARG_LEN 256
#define MAX_ERROR_MSG 512
#define SOCKET_PATH "/tmp/process_orchestrator.sock"

typedef enum {
    MSG_START_PROCESS = 1,
    MSG_PROCESS_STARTED,
    MSG_PROCESS_ERROR,
    MSG_ACK,
    MSG_LIST_PROCESSES,
    MSG_PROCESS_LIST,
    MSG_STOP_PROCESS,
    MSG_PROCESS_STOPPED,
    MSG_APPLY_CONSTRAINTS,
    MSG_CONSTRAINTS_APPLIED,
    MSG_PING,
    MSG_PONG
} message_type_t;

typedef struct {
    uint32_t type;
    uint32_t length;
} message_header_t;

typedef struct {
    char name[MAX_PROCESS_NAME];
    char args[MAX_ARGS][MAX_ARG_LEN];
    int arg_count;
} start_process_msg_t;

typedef struct {
    pid_t host_pid;
    pid_t container_pid;  // PID as seen inside container namespace
} process_started_msg_t;

typedef struct {
    char error[MAX_ERROR_MSG];
} process_error_msg_t;

typedef struct {
    uint32_t request_id;
} ack_msg_t;

typedef struct {
    pid_t pid;
} stop_process_msg_t;

typedef struct {
    pid_t pid;
} process_stopped_msg_t;

typedef struct {
    pid_t pid;
    uint64_t memory_limit;
    uint64_t cpu_limit;
} apply_constraints_msg_t;

typedef struct {
    pid_t pid;
} constraints_applied_msg_t;

typedef struct {
    int count;
    struct {
        pid_t host_pid;
        pid_t container_pid;  // PID as seen inside container namespace
        char name[MAX_PROCESS_NAME];
    } processes[64];
} process_list_msg_t;

typedef struct {
    message_header_t header;
    union {
        start_process_msg_t start_process;
        process_started_msg_t process_started;
        process_error_msg_t process_error;
        ack_msg_t ack;
        stop_process_msg_t stop_process;
        process_stopped_msg_t process_stopped;
        apply_constraints_msg_t apply_constraints;
        constraints_applied_msg_t constraints_applied;
        process_list_msg_t process_list;
    } data;
} message_t;

int send_message(int sockfd, const message_t *msg);
int recv_message(int sockfd, message_t *msg);

#endif