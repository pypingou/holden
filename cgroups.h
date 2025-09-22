#ifndef CGROUPS_H
#define CGROUPS_H

#include <sys/types.h>
#include <stdint.h>

#define CGROUP_BASE_PATH "/sys/fs/cgroup"
#define CGROUP_ORCHESTRATOR_PATH "/sys/fs/cgroup/orchestrator"

int init_cgroups();
int create_process_cgroup(pid_t pid);
int apply_memory_limit(pid_t pid, uint64_t memory_bytes);
int apply_cpu_limit(pid_t pid, uint64_t cpu_percent);
int add_process_to_cgroup(pid_t pid);
int cleanup_process_cgroup(pid_t pid);

#endif