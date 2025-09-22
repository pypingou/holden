#include "cgroups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

static int write_to_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        return -1;
    }

    ssize_t len = strlen(value);
    ssize_t written = write(fd, value, len);
    close(fd);

    return (written == len) ? 0 : -1;
}

static int create_directory_if_not_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }

    return mkdir(path, 0755);
}

int init_cgroups() {
    if (create_directory_if_not_exists(CGROUP_ORCHESTRATOR_PATH) == -1) {
        fprintf(stderr, "Failed to create orchestrator cgroup directory: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int create_process_cgroup(pid_t pid) {
    char cgroup_path[512];
    snprintf(cgroup_path, sizeof(cgroup_path), "%s/proc_%d", CGROUP_ORCHESTRATOR_PATH, pid);

    if (create_directory_if_not_exists(cgroup_path) == -1) {
        fprintf(stderr, "Failed to create process cgroup directory %s: %s\n",
                cgroup_path, strerror(errno));
        return -1;
    }

    return add_process_to_cgroup(pid);
}

int add_process_to_cgroup(pid_t pid) {
    char cgroup_procs_path[512];
    char pid_str[32];

    snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
             "%s/proc_%d/cgroup.procs", CGROUP_ORCHESTRATOR_PATH, pid);
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    return write_to_file(cgroup_procs_path, pid_str);
}

int apply_memory_limit(pid_t pid, uint64_t memory_bytes) {
    char memory_limit_path[512];
    char limit_str[64];

    snprintf(memory_limit_path, sizeof(memory_limit_path),
             "%s/proc_%d/memory.max", CGROUP_ORCHESTRATOR_PATH, pid);
    snprintf(limit_str, sizeof(limit_str), "%llu", (unsigned long long)memory_bytes);

    return write_to_file(memory_limit_path, limit_str);
}

int apply_cpu_limit(pid_t pid, uint64_t cpu_percent) {
    char cpu_weight_path[512];
    char weight_str[32];

    if (cpu_percent > 100) {
        cpu_percent = 100;
    }

    uint64_t weight = (cpu_percent * 10000) / 100;

    snprintf(cpu_weight_path, sizeof(cpu_weight_path),
             "%s/proc_%d/cpu.weight", CGROUP_ORCHESTRATOR_PATH, pid);
    snprintf(weight_str, sizeof(weight_str), "%llu", (unsigned long long)weight);

    return write_to_file(cpu_weight_path, weight_str);
}

int cleanup_process_cgroup(pid_t pid) {
    char cgroup_path[512];
    snprintf(cgroup_path, sizeof(cgroup_path), "%s/proc_%d", CGROUP_ORCHESTRATOR_PATH, pid);

    return rmdir(cgroup_path);
}