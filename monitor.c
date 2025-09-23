#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "protocol.h"

static int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
    }
}

int read_proc_stat(pid_t pid, unsigned long *utime, unsigned long *stime) {
    char path[256];
    char buffer[1024];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *token = strtok(buffer, " ");
    for (int i = 0; i < 13 && token; i++) {
        token = strtok(NULL, " ");
    }

    if (token) {
        *utime = strtoul(token, NULL, 10);
        token = strtok(NULL, " ");
        if (token) {
            *stime = strtoul(token, NULL, 10);
            return 0;
        }
    }

    return -1;
}

int read_proc_status(pid_t pid, unsigned long *vmrss_kb) {
    char path[256];
    char buffer[256];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strncmp(buffer, "VmRSS:", 6) == 0) {
            sscanf(buffer, "VmRSS: %lu kB", vmrss_kb);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

int check_process_exists(pid_t pid) {
    return kill(pid, 0) == 0;
}

void monitor_processes() {
    int sockfd;
    struct sockaddr_un addr;
    const char *socket_path;

    // Allow socket path to be configured via environment variable
    socket_path = getenv("HOLDEN_SOCKET_PATH");
    if (socket_path == NULL) {
        socket_path = SOCKET_PATH;  // Fall back to default
    }

    while (running) {
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1) {
            sleep(5);
            continue;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(sockfd);
            printf("Agent not available, retrying in 5 seconds...\n");
            sleep(5);
            continue;
        }

        message_t request = {0};
        request.header.type = MSG_LIST_PROCESSES;
        request.header.length = 0;

        if (send_message(sockfd, &request) == -1) {
            close(sockfd);
            sleep(5);
            continue;
        }

        message_t response;
        if (recv_message(sockfd, &response) == -1) {
            close(sockfd);
            sleep(5);
            continue;
        }

        if (response.header.type == MSG_PROCESS_LIST) {
            time_t now = time(NULL);
            char timestr[64];
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));

            printf("\n[%s] Process Monitor Report\n", timestr);
            printf("========================================\n");

            if (response.data.process_list.count == 0) {
                printf("No processes running.\n");
            } else {
                for (int i = 0; i < response.data.process_list.count; i++) {
                    pid_t pid = response.data.process_list.processes[i].pid;
                    const char *name = response.data.process_list.processes[i].name;

                    unsigned long utime, stime, vmrss_kb;
                    int cpu_available = (read_proc_stat(pid, &utime, &stime) == 0);
                    int mem_available = (read_proc_status(pid, &vmrss_kb) == 0);

                    printf("Process %d (%s):\n", pid, name);
                    printf("  Status: %s\n", check_process_exists(pid) ? "Running" : "Dead");

                    if (cpu_available) {
                        printf("  CPU Time: user=%lu, system=%lu\n", utime, stime);
                    } else {
                        printf("  CPU Time: unavailable\n");
                    }

                    if (mem_available) {
                        printf("  Memory: %lu KB (%.2f MB)\n", vmrss_kb, vmrss_kb / 1024.0);
                    } else {
                        printf("  Memory: unavailable\n");
                    }

                    printf("\n");
                }
            }
        }

        close(sockfd);
        sleep(10);
    }
}

void print_monitor_usage(const char *prog_name) {
    printf("Holden Process Monitor\n");
    printf("Named after 19th century puppeteer Joseph Holden\n");
    printf("\n");
    printf("Usage: %s [--help|-h]\n", prog_name);
    printf("\n");
    printf("Continuously monitors processes managed by the Holden agent.\n");
    printf("Displays real-time information about process status, CPU usage, and memory consumption.\n");
    printf("\n");
    printf("Environment Variables:\n");
    printf("  HOLDEN_SOCKET_PATH    - Path to agent socket (default: %s)\n", SOCKET_PATH);
    printf("\n");
    printf("Features:\n");
    printf("  - Real-time process status monitoring\n");
    printf("  - CPU time tracking (user/system)\n");
    printf("  - Memory usage statistics (RSS)\n");
    printf("  - Automatic reconnection to agent\n");
    printf("  - Timestamped reports every 10 seconds\n");
    printf("\n");
    printf("Control:\n");
    printf("  Press Ctrl+C to stop monitoring\n");
    printf("\n");
    printf("Note: The agent must be running for monitoring to work.\n");
}

int main(int argc, char *argv[]) {
    // Handle --help or -h
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_monitor_usage(argv[0]);
        return 0;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Process Monitor starting...\n");
    printf("Press Ctrl+C to stop\n");

    monitor_processes();

    printf("Process Monitor stopping...\n");
    return 0;
}