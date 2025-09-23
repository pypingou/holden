#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include "protocol.h"

// Signal handler for SIGCHLD to reap zombie children
void sigchld_handler(int sig) {
    (void)sig; // Unused parameter
    int status;
    pid_t pid;

    // Reap all available zombie children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Child reaped, no action needed since we use pidfds for monitoring
    }
}

// pidfd_open system call wrapper
int pidfd_open(pid_t pid, unsigned int flags) {
    return syscall(SYS_pidfd_open, pid, flags);
}

// Receive file descriptor over Unix socket
int recv_fd(int socket) {
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(int))];
    char data;
    struct iovec iov = {.iov_base = &data, .iov_len = 1};

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    if (recvmsg(socket, &msg, 0) < 0) {
        return -1;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        return *(int*)CMSG_DATA(cmsg);
    }

    return -1;
}

// Connect to the agent
int connect_to_agent() {
    int sockfd;
    struct sockaddr_un addr;
    const char *socket_path;

    socket_path = getenv("HOLDEN_SOCKET_PATH");
    if (socket_path == NULL) {
        socket_path = SOCKET_PATH;
    }

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect to agent");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Spawn a process locally using fork() and return its pidfd
int spawn_local_process(const char *cmd, char *const args[]) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child: close inherited fds and exec
        for (int fd = 3; fd < 1024; fd++) {
            close(fd);
        }

        execvp(cmd, args);
        fprintf(stderr, "Failed to exec %s: %s\n", cmd, strerror(errno));
        _exit(1);
    }

    // Parent: get pidfd for child
    int pidfd = pidfd_open(pid, 0);
    if (pidfd == -1) {
        perror("pidfd_open for local process");
        return -1;
    }

    printf("Spawned local process %s with PID %d, pidfd %d\n", cmd, pid, pidfd);
    return pidfd;
}

// Spawn a process via the agent and return its pidfd
int spawn_agent_process(const char *cmd, char *const args[]) {
    int sockfd = connect_to_agent();
    if (sockfd == -1) {
        return -1;
    }

    // Prepare start process message
    message_t request = {0};
    request.header.type = MSG_START_PROCESS;
    request.header.length = sizeof(start_process_msg_t);

    strncpy(request.data.start_process.name, cmd, MAX_PROCESS_NAME - 1);
    request.data.start_process.name[MAX_PROCESS_NAME - 1] = '\0';

    // Copy arguments (skip argv[0] since we already have the command name)
    int arg_count = 0;
    for (int i = 1; args[i] != NULL && arg_count < MAX_ARGS; i++) {
        strncpy(request.data.start_process.args[arg_count], args[i], MAX_ARG_LEN - 1);
        request.data.start_process.args[arg_count][MAX_ARG_LEN - 1] = '\0';
        arg_count++;
    }
    request.data.start_process.arg_count = arg_count;

    // Send request to agent
    if (send_message(sockfd, &request) == -1) {
        perror("send_message to agent");
        close(sockfd);
        return -1;
    }

    // Receive response
    message_t response;
    if (recv_message(sockfd, &response) == -1) {
        perror("recv_message from agent");
        close(sockfd);
        return -1;
    }

    if (response.header.type != MSG_PROCESS_STARTED) {
        fprintf(stderr, "Agent failed to start process: %s\n",
                response.data.process_error.error);
        close(sockfd);
        return -1;
    }

    // Receive the pidfd via fd passing
    int pidfd = recv_fd(sockfd);
    if (pidfd == -1) {
        perror("recv_fd from agent");
        close(sockfd);
        return -1;
    }

    printf("Spawned agent process %s with PID %d, pidfd %d\n",
           cmd, response.data.process_started.host_pid, pidfd);

    close(sockfd);
    return pidfd;
}

void print_usage(const char *prog_name) {
    printf("Holden PID File Descriptor Process Orchestrator\n");
    printf("Usage: %s <local_cmd> <agent_cmd>\n", prog_name);
    printf("\n");
    printf("This program demonstrates pidfd-based process orchestration by:\n");
    printf("1. Spawning <local_cmd> locally using fork() and getting its pidfd\n");
    printf("2. Spawning <agent_cmd> via the holden agent and receiving its pidfd\n");
    printf("3. Orchestrating both processes using poll() on their pidfds\n");
    printf("4. Automatically restarting processes when they die\n");
    printf("\n");
    printf("Example: %s 'sleep 5' 'sleep 10'\n", prog_name);
    printf("Environment Variables:\n");
    printf("  HOLDEN_SOCKET_PATH - Path to agent socket (default: %s)\n", SOCKET_PATH);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    // Install SIGCHLD handler to reap zombie children
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    const char *local_cmd = argv[1];
    const char *agent_cmd = argv[2];

    // Parse commands into argv arrays (simple space-based parsing)
    char local_cmd_copy[256], agent_cmd_copy[256];
    strncpy(local_cmd_copy, local_cmd, sizeof(local_cmd_copy) - 1);
    local_cmd_copy[sizeof(local_cmd_copy) - 1] = '\0';
    strncpy(agent_cmd_copy, agent_cmd, sizeof(agent_cmd_copy) - 1);
    agent_cmd_copy[sizeof(agent_cmd_copy) - 1] = '\0';

    // Simple tokenization for local command
    char *local_args[16] = {NULL};
    int local_argc = 0;
    char *token = strtok(local_cmd_copy, " ");
    while (token && local_argc < 15) {
        local_args[local_argc++] = token;
        token = strtok(NULL, " ");
    }
    local_args[local_argc] = NULL;

    // Simple tokenization for agent command
    char *agent_args[16] = {NULL};
    int agent_argc = 0;
    token = strtok(agent_cmd_copy, " ");
    while (token && agent_argc < 15) {
        agent_args[agent_argc++] = token;
        token = strtok(NULL, " ");
    }
    agent_args[agent_argc] = NULL;

    printf("Starting pidfd orchestrator demo...\n");
    printf("Local command: %s\n", local_cmd);
    printf("Agent command: %s\n", agent_cmd);
    printf("Press Ctrl+C to exit\n\n");

    int local_pidfd = -1;
    int agent_pidfd = -1;
    int restart_count = 0;

    // Initial spawn
    local_pidfd = spawn_local_process(local_args[0], local_args);
    if (local_pidfd == -1) {
        fprintf(stderr, "Failed to spawn local process\n");
        return 1;
    }

    agent_pidfd = spawn_agent_process(agent_args[0], agent_args);
    if (agent_pidfd == -1) {
        fprintf(stderr, "Failed to spawn agent process\n");
        close(local_pidfd);
        return 1;
    }

    // Monitor loop
    while (1) {
        struct pollfd fds[2];
        fds[0].fd = local_pidfd;
        fds[0].events = POLLIN;
        fds[1].fd = agent_pidfd;
        fds[1].events = POLLIN;

        printf("Monitoring processes (restart count: %d)...\n", restart_count);

        int ret = poll(fds, 2, -1);  // Block indefinitely
        if (ret == -1) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal, continue monitoring
            }
            perror("poll");
            break;
        }

        time_t now = time(NULL);

        // Check if local process died
        if (fds[0].revents & POLLIN) {
            char *time_str = ctime(&now);
            time_str[strlen(time_str) - 1] = '\0'; // Remove newline
            printf("[%s] Local process died, restarting...\n", time_str);
            close(local_pidfd);
            local_pidfd = spawn_local_process(local_args[0], local_args);
            if (local_pidfd == -1) {
                fprintf(stderr, "Failed to restart local process\n");
                break;
            }
            restart_count++;
        }

        // Check if agent process died
        if (fds[1].revents & POLLIN) {
            char *time_str = ctime(&now);
            time_str[strlen(time_str) - 1] = '\0'; // Remove newline
            printf("[%s] Agent process died, restarting...\n", time_str);
            close(agent_pidfd);
            agent_pidfd = spawn_agent_process(agent_args[0], agent_args);
            if (agent_pidfd == -1) {
                fprintf(stderr, "Failed to restart agent process\n");
                break;
            }
            restart_count++;
        }

        // Small delay to avoid tight restart loops
        usleep(100000);  // 100ms
    }

    // Cleanup
    if (local_pidfd != -1) {
        close(local_pidfd);
    }
    if (agent_pidfd != -1) {
        close(agent_pidfd);
    }

    printf("Monitor exiting after %d restarts\n", restart_count);
    return 0;
}