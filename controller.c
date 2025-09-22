#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include "protocol.h"

typedef struct {
    pid_t pid;
    char name[MAX_PROCESS_NAME];
    time_t start_time;
    int monitored;
} monitored_process_t;

static monitored_process_t monitored_processes[64];
static int monitored_count = 0;

int connect_to_agent() {
    int sockfd;
    struct sockaddr_un addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void add_monitored_process(pid_t pid, const char *name) {
    if (monitored_count < 64) {
        monitored_processes[monitored_count].pid = pid;
        strncpy(monitored_processes[monitored_count].name, name, MAX_PROCESS_NAME - 1);
        monitored_processes[monitored_count].name[MAX_PROCESS_NAME - 1] = '\0';
        monitored_processes[monitored_count].start_time = time(NULL);
        monitored_processes[monitored_count].monitored = 1;
        monitored_count++;
        printf("Now monitoring process %d (%s)\n", pid, name);
    }
}

void remove_monitored_process(pid_t pid) {
    for (int i = 0; i < monitored_count; i++) {
        if (monitored_processes[i].pid == pid && monitored_processes[i].monitored) {
            monitored_processes[i].monitored = 0;
            printf("Stopped monitoring process %d\n", pid);
            break;
        }
    }
}

int start_process_cmd(const char *name, char *args[], int arg_count) {
    int sockfd = connect_to_agent();
    if (sockfd == -1) {
        printf("Failed to connect to agent\n");
        return -1;
    }

    message_t request = {0};
    request.header.type = MSG_START_PROCESS;
    request.header.length = sizeof(start_process_msg_t);

    strncpy(request.data.start_process.name, name, MAX_PROCESS_NAME - 1);
    request.data.start_process.name[MAX_PROCESS_NAME - 1] = '\0';
    request.data.start_process.arg_count = arg_count;

    for (int i = 0; i < arg_count && i < MAX_ARGS; i++) {
        strncpy(request.data.start_process.args[i], args[i], MAX_ARG_LEN - 1);
        request.data.start_process.args[i][MAX_ARG_LEN - 1] = '\0';
    }

    if (send_message(sockfd, &request) == -1) {
        printf("Failed to send start process message\n");
        close(sockfd);
        return -1;
    }

    message_t response;
    if (recv_message(sockfd, &response) == -1) {
        printf("Failed to receive response\n");
        close(sockfd);
        return -1;
    }

    if (response.header.type == MSG_PROCESS_STARTED) {
        printf("Process started successfully with PID: %d\n", response.data.process_started.pid);
        add_monitored_process(response.data.process_started.pid, name);

        message_t ack = {0};
        ack.header.type = MSG_ACK;
        ack.header.length = sizeof(ack_msg_t);
        ack.data.ack.request_id = response.data.process_started.pid;
        send_message(sockfd, &ack);
    } else if (response.header.type == MSG_PROCESS_ERROR) {
        printf("Error starting process: %s\n", response.data.process_error.error);
    }

    close(sockfd);
    return 0;
}

int list_processes_cmd() {
    int sockfd = connect_to_agent();
    if (sockfd == -1) {
        printf("Failed to connect to agent\n");
        return -1;
    }

    message_t request = {0};
    request.header.type = MSG_LIST_PROCESSES;
    request.header.length = 0;

    if (send_message(sockfd, &request) == -1) {
        printf("Failed to send list processes message\n");
        close(sockfd);
        return -1;
    }

    message_t response;
    if (recv_message(sockfd, &response) == -1) {
        printf("Failed to receive response\n");
        close(sockfd);
        return -1;
    }

    if (response.header.type == MSG_PROCESS_LIST) {
        printf("Running processes (%d):\n", response.data.process_list.count);
        for (int i = 0; i < response.data.process_list.count; i++) {
            printf("  PID: %d, Name: %s\n",
                   response.data.process_list.processes[i].pid,
                   response.data.process_list.processes[i].name);
        }
    }

    close(sockfd);
    return 0;
}

int stop_process_cmd(pid_t pid) {
    int sockfd = connect_to_agent();
    if (sockfd == -1) {
        printf("Failed to connect to agent\n");
        return -1;
    }

    message_t request = {0};
    request.header.type = MSG_STOP_PROCESS;
    request.header.length = sizeof(stop_process_msg_t);
    request.data.stop_process.pid = pid;

    if (send_message(sockfd, &request) == -1) {
        printf("Failed to send stop process message\n");
        close(sockfd);
        return -1;
    }

    message_t response;
    if (recv_message(sockfd, &response) == -1) {
        printf("Failed to receive response\n");
        close(sockfd);
        return -1;
    }

    if (response.header.type == MSG_PROCESS_STOPPED) {
        printf("Process %d stopped successfully\n", response.data.process_stopped.pid);
        remove_monitored_process(pid);
    } else if (response.header.type == MSG_PROCESS_ERROR) {
        printf("Error stopping process: %s\n", response.data.process_error.error);
    }

    close(sockfd);
    return 0;
}

int apply_constraints_cmd(pid_t pid, uint64_t memory_limit, uint64_t cpu_limit) {
    int sockfd = connect_to_agent();
    if (sockfd == -1) {
        printf("Failed to connect to agent\n");
        return -1;
    }

    message_t request = {0};
    request.header.type = MSG_APPLY_CONSTRAINTS;
    request.header.length = sizeof(apply_constraints_msg_t);
    request.data.apply_constraints.pid = pid;
    request.data.apply_constraints.memory_limit = memory_limit;
    request.data.apply_constraints.cpu_limit = cpu_limit;

    if (send_message(sockfd, &request) == -1) {
        printf("Failed to send apply constraints message\n");
        close(sockfd);
        return -1;
    }

    message_t response;
    if (recv_message(sockfd, &response) == -1) {
        printf("Failed to receive response\n");
        close(sockfd);
        return -1;
    }

    if (response.header.type == MSG_CONSTRAINTS_APPLIED) {
        printf("Constraints applied to process %d\n", response.data.constraints_applied.pid);
    } else if (response.header.type == MSG_PROCESS_ERROR) {
        printf("Error applying constraints: %s\n", response.data.process_error.error);
    }

    close(sockfd);
    return 0;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s <command> [args...]\n", prog_name);
    printf("Commands:\n");
    printf("  start <name> [args...]     - Start a process\n");
    printf("  list                       - List running processes\n");
    printf("  stop <pid>                 - Stop a process\n");
    printf("  constrain <pid> <mem> <cpu> - Apply constraints (mem in MB, cpu in %%)\n");
    printf("  monitor                    - Show monitored processes\n");
}

void show_monitored_processes() {
    printf("Monitored processes (%d):\n", monitored_count);
    for (int i = 0; i < monitored_count; i++) {
        if (monitored_processes[i].monitored) {
            time_t now = time(NULL);
            int uptime = (int)(now - monitored_processes[i].start_time);
            printf("  PID: %d, Name: %s, Uptime: %d seconds\n",
                   monitored_processes[i].pid,
                   monitored_processes[i].name,
                   uptime);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        if (argc < 3) {
            printf("Usage: %s start <name> [args...]\n", argv[0]);
            return 1;
        }
        return start_process_cmd(argv[2], &argv[3], argc - 3);
    }
    else if (strcmp(argv[1], "list") == 0) {
        return list_processes_cmd();
    }
    else if (strcmp(argv[1], "stop") == 0) {
        if (argc != 3) {
            printf("Usage: %s stop <pid>\n", argv[0]);
            return 1;
        }
        return stop_process_cmd(atoi(argv[2]));
    }
    else if (strcmp(argv[1], "constrain") == 0) {
        if (argc != 5) {
            printf("Usage: %s constrain <pid> <memory_mb> <cpu_percent>\n", argv[0]);
            return 1;
        }
        return apply_constraints_cmd(atoi(argv[2]), atoll(argv[3]) * 1024 * 1024, atoll(argv[4]));
    }
    else if (strcmp(argv[1], "monitor") == 0) {
        show_monitored_processes();
        return 0;
    }
    else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
}