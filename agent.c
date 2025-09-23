#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <signal.h>
#include "protocol.h"

// Signal handler for SIGCHLD to reap zombie children
void sigchld_handler(int sig) {
    (void)sig; // Unused parameter
    int status;
    pid_t pid;

    // Reap all available zombie children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Child reaped, no action needed since caller manages processes via pidfds
    }
}

static const char *current_socket_path = NULL;

// pidfd_open system call wrapper
int pidfd_open(pid_t pid, unsigned int flags) {
    return syscall(SYS_pidfd_open, pid, flags);
}

// Send file descriptor over Unix socket
int send_fd(int socket, int fd) {
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    char data = 'x';
    struct iovec iov = {.iov_base = &data, .iov_len = 1};

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int*)CMSG_DATA(cmsg) = fd;

    return sendmsg(socket, &msg, 0);
}

int start_process(int sockfd, const start_process_msg_t *req, message_t *response) {
    pid_t pid = fork();

    if (pid == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to fork: %s", strerror(errno));
        return 0;
    }

    if (pid == 0) {
        // Child process: close inherited file descriptors and exec
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
        _exit(1);
    }

    // Parent: get pidfd for the child
    int pidfd = pidfd_open(pid, 0);
    if (pidfd == -1) {
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to open pidfd for process %d: %s", pid, strerror(errno));
        return 0;
    }

    // Prepare success response first
    response->header.type = MSG_PROCESS_STARTED;
    response->header.length = sizeof(process_started_msg_t);
    response->data.process_started.host_pid = pid;
    response->data.process_started.container_pid = pid;

    // Send the response first
    if (send_message(sockfd, response) == -1) {
        close(pidfd);
        response->header.type = MSG_PROCESS_ERROR;
        response->header.length = sizeof(process_error_msg_t);
        snprintf(response->data.process_error.error, MAX_ERROR_MSG,
                "Failed to send response: %s", strerror(errno));
        return 0;
    }

    // Then send the pidfd via fd passing
    if (send_fd(sockfd, pidfd) == -1) {
        close(pidfd);
        // Can't send error response now since we already sent success
        // The orchestrator will get an error when trying to recv_fd
        return -1;
    }

    close(pidfd); // We've passed it, don't need our copy

    // Return 1 to indicate we already sent the response
    return 1;
}

int handle_message(int sockfd, const message_t *request) {
    message_t response = {0};

    switch (request->header.type) {
        case MSG_START_PROCESS: {
            int result = start_process(sockfd, &request->data.start_process, &response);
            if (result == 1) {
                // start_process already sent the response, don't send again
                return 0;
            } else if (result == -1) {
                // Error occurred after response was sent
                return -1;
            }
            // result == 0: error response prepared, send it below
            break;
        }

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
        unlink(current_socket_path);
    }
}

void print_agent_usage(const char *prog_name) {
    printf("Holden Process Orchestration Agent\n");
    printf("Named after 19th century puppeteer Joseph Holden\n");
    printf("\n");
    printf("Usage: %s [--help|-h]\n", prog_name);
    printf("\n");
    printf("The agent spawns processes and returns pidfd references.\n");
    printf("It listens on a Unix domain socket for commands from holden-controller.\n");
    printf("\n");
    printf("Environment Variables:\n");
    printf("  HOLDEN_SOCKET_PATH    - Path to agent socket (default: %s)\n", SOCKET_PATH);
    printf("\n");
    printf("The agent maintains no state - all process management is handled by the caller.\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_agent_usage(argv[0]);
        return 0;
    }

    int sockfd, clientfd;
    struct sockaddr_un addr;
    const char *socket_path;

    signal(SIGPIPE, SIG_IGN);
    atexit(cleanup_socket);

    // Install SIGCHLD handler to reap zombie children
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    socket_path = getenv("HOLDEN_SOCKET_PATH");
    if (socket_path == NULL) {
        socket_path = SOCKET_PATH;
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