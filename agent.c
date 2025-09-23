#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include "protocol.h"
#include "cgroups.h"

#define MAX_PROCESSES 64

typedef struct {
    pid_t pid;
    char name[MAX_PROCESS_NAME];
    int active;
} process_info_t;

static process_info_t processes[MAX_PROCESSES];
static int process_count = 0;
static const char *current_socket_path = NULL;

// Forward declarations
void remove_process(pid_t pid);

// Function to read container PID from /proc/{pid}/status
pid_t get_container_pid(pid_t host_pid) {
    char path[256];
    char buffer[1024];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/status", host_pid);
    fp = fopen(path, "r");
    if (!fp) {
        return host_pid;  // Fall back to host PID if we can't read status
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strncmp(buffer, "NSpid:", 6) == 0) {
            // NSpid: format is "NSpid:\t<host_pid>\t<container_pid>"
            // We want the last PID in the list (innermost namespace)
            char *token = strtok(buffer + 6, " \t\n");
            pid_t container_pid = host_pid;  // Default to host PID

            while (token) {
                container_pid = atoi(token);
                token = strtok(NULL, " \t\n");
            }

            fclose(fp);
            return container_pid;
        }
    }

    fclose(fp);
    return host_pid;  // If no NSpid found, return host PID
}

void sigchld_handler(int sig) {
    (void)sig;  // Unused parameter
    // Reap all available zombie children without affecting the agent
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fprintf(stderr, "DEBUG: Reaped child process %d\n", pid);
        // Also remove from tracking if it's a managed process
        remove_process(pid);
    }
}

void add_process(pid_t pid, const char *name) {
    if (process_count < MAX_PROCESSES) {
        processes[process_count].pid = pid;
        strncpy(processes[process_count].name, name, MAX_PROCESS_NAME - 1);
        processes[process_count].name[MAX_PROCESS_NAME - 1] = '\0';
        processes[process_count].active = 1;
        process_count++;
    }
}

void remove_process(pid_t pid) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].pid == pid && processes[i].active) {
            processes[i].active = 0;
            break;
        }
    }
}

process_info_t* find_process(pid_t pid) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].pid == pid && processes[i].active) {
            return &processes[i];
        }
    }
    return NULL;
}

int start_process(const start_process_msg_t *req, message_t *response) {
    pid_t pid = fork();

    if (pid == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to fork: %s", strerror(errno));
        return 0;
    }

    if (pid == 0) {
        // Child process: clear inherited atexit handlers to prevent socket cleanup
        // This is critical - child must not clean up parent's socket!

        // Close all inherited file descriptors
        // This prevents interference with parent's socket operations
        for (int fd = 3; fd < 1024; fd++) {
            close(fd);
        }

        char *args[MAX_ARGS + 2];
        args[0] = (char*)req->name;

        for (int i = 0; i < req->arg_count && i < MAX_ARGS; i++) {
            args[i + 1] = (char*)req->args[i];
        }
        args[req->arg_count + 1] = NULL;

        execvp(req->name, args);
        fprintf(stderr, "Failed to exec %s: %s\n", req->name, strerror(errno));
        _exit(1);  // Use _exit to avoid calling atexit handlers
    }

    add_process(pid, req->name);

    // Get container PID (might be different due to PID namespaces)
    pid_t container_pid = get_container_pid(pid);
    fprintf(stderr, "DEBUG: Started process %d (%s), container PID: %d\n", pid, req->name, container_pid);

    response->header.type = MSG_PROCESS_STARTED;
    response->header.length = sizeof(process_started_msg_t);
    response->data.process_started.host_pid = pid;
    response->data.process_started.container_pid = container_pid;

    return 0;
}

int stop_process(const stop_process_msg_t *req, message_t *response) {
    process_info_t *proc = find_process(req->pid);

    if (!proc) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Process %d not found", req->pid);
        return 0;
    }

    if (kill(req->pid, SIGTERM) == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to kill process %d: %s", req->pid, strerror(errno));
        return 0;
    }

    // Don't remove from tracking immediately - let SIGCHLD handler do it when process actually exits
    // This prevents zombie processes when stop_process is called

    response->header.type = MSG_PROCESS_STOPPED;
    response->header.length = sizeof(process_stopped_msg_t);
    response->data.process_stopped.pid = req->pid;

    return 0;
}

int list_processes(message_t *response) {
    response->header.type = MSG_PROCESS_LIST;
    response->header.length = sizeof(process_list_msg_t);

    int active_count = 0;
    for (int i = 0; i < process_count && active_count < 64; i++) {
        if (processes[i].active) {
            int status;
            if (waitpid(processes[i].pid, &status, WNOHANG) == 0) {
                pid_t container_pid = get_container_pid(processes[i].pid);
                response->data.process_list.processes[active_count].host_pid = processes[i].pid;
                response->data.process_list.processes[active_count].container_pid = container_pid;
                strncpy(response->data.process_list.processes[active_count].name,
                       processes[i].name, MAX_PROCESS_NAME - 1);
                response->data.process_list.processes[active_count].name[MAX_PROCESS_NAME - 1] = '\0';
                active_count++;
            } else {
                processes[i].active = 0;
            }
        }
    }

    response->data.process_list.count = active_count;
    return 0;
}

int apply_constraints(const apply_constraints_msg_t *req, message_t *response) {
    process_info_t *proc = find_process(req->pid);

    if (!proc) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Process %d not found", req->pid);
        return 0;
    }

    if (create_process_cgroup(req->pid) == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to create cgroup for process %d", req->pid);
        return 0;
    }

    if (req->memory_limit > 0 && apply_memory_limit(req->pid, req->memory_limit) == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to apply memory limit to process %d", req->pid);
        return 0;
    }

    if (req->cpu_limit > 0 && apply_cpu_limit(req->pid, req->cpu_limit) == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to apply CPU limit to process %d", req->pid);
        return 0;
    }

    response->header.type = MSG_CONSTRAINTS_APPLIED;
    response->header.length = sizeof(constraints_applied_msg_t);
    response->data.constraints_applied.pid = req->pid;

    return 0;
}

int handle_message(int sockfd, const message_t *request) {
    message_t response = {0};

    switch (request->header.type) {
        case MSG_START_PROCESS:
            start_process(&request->data.start_process, &response);
            break;

        case MSG_STOP_PROCESS:
            stop_process(&request->data.stop_process, &response);
            break;

        case MSG_LIST_PROCESSES:
            list_processes(&response);
            break;

        case MSG_APPLY_CONSTRAINTS:
            apply_constraints(&request->data.apply_constraints, &response);
            break;

        case MSG_PING:
            response.header.type = MSG_PONG;
            response.header.length = 0;
            break;

        default:
            response.header.type = MSG_PROCESS_ERROR;
            response.header.length = sizeof(process_error_msg_t);
            snprintf(response.data.process_error.error, MAX_ERROR_MSG,
                    "Unknown message type: %d", request->header.type);
            break;
    }

    return send_message(sockfd, &response);
}

void cleanup_socket() {
    if (current_socket_path) {
        fprintf(stderr, "DEBUG: cleanup_socket() called - removing %s\n", current_socket_path);
        unlink(current_socket_path);
    }
}

void print_agent_usage(const char *prog_name) {
    printf("Holden Process Orchestration Agent\n");
    printf("Named after 19th century puppeteer Joseph Holden\n");
    printf("\n");
    printf("Usage: %s [--help|-h]\n", prog_name);
    printf("\n");
    printf("The agent runs as a daemon and manages processes on behalf of controllers.\n");
    printf("It listens on a Unix domain socket for commands from holden-controller.\n");
    printf("\n");
    printf("Environment Variables:\n");
    printf("  HOLDEN_SOCKET_PATH    - Path to agent socket (default: %s)\n", SOCKET_PATH);
    printf("\n");
    printf("Features:\n");
    printf("  - Process lifecycle management (start/stop/list)\n");
    printf("  - Resource constraints via cgroups v2\n");
    printf("  - Real-time process monitoring\n");
    printf("  - Unix domain socket communication\n");
    printf("\n");
    printf("Typically started via systemd: systemctl start holden-agent\n");
}

int main(int argc, char *argv[]) {
    // Handle --help or -h
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_agent_usage(argv[0]);
        return 0;
    }

    int sockfd, clientfd;
    struct sockaddr_un addr;
    const char *socket_path;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);  // Handle child process exits properly
    atexit(cleanup_socket);

    if (init_cgroups() == -1) {
        fprintf(stderr, "Warning: Failed to initialize cgroups, constraints will not work\n");
    }

    // Allow socket path to be configured via environment variable
    socket_path = getenv("HOLDEN_SOCKET_PATH");
    if (socket_path == NULL) {
        socket_path = SOCKET_PATH;  // Fall back to default
    }
    current_socket_path = socket_path;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    unlink(socket_path);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Agent listening on %s\n", socket_path);

    while (1) {
        clientfd = accept(sockfd, NULL, NULL);
        if (clientfd == -1) {
            perror("accept");
            continue;
        }

        message_t request;
        while (recv_message(clientfd, &request) == 0) {
            if (handle_message(clientfd, &request) == -1) {
                break;
            }
        }

        close(clientfd);
    }

    close(sockfd);
    return 0;
}