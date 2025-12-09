#define _GNU_SOURCE
/*
 * SysMonitor++ - Linux System Resource Monitoring Tool
 * A lightweight system monitor using /proc filesystem and system calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#define BUFFER_SIZE 4096

// Global flag for signal handling
volatile sig_atomic_t keep_running = 1;

// Structures
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CPUStats;

typedef struct {
    int pid;
    char name[256];
    unsigned long long cpu_time; // utime + stime
} ProcessInfo;

// Function Prototypes
void displayMenu();
void getCPUUsage();
void getMemoryUsage();
void listTopProcesses();
void continuousMonitor(int interval);
void handleSignal(int sig);
void log_message(const char *message);
int get_input_int(const char *prompt);
void clear_screen();
void read_cpu_stats(CPUStats *stats);

// Async-signal-safe signal handler
void handleSignal(int sig) {
    if (sig == SIGINT) {
        keep_running = 0;
        const char *msg = "\nCaught SIGINT. Exiting gracefully...\n";
        // Use write() as it is async-signal-safe, printf is not
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

// Log message to syslog.txt with timestamp
void log_message(const char *message) {
    int fd = open("syslog.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening syslog.txt");
        return;
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = 0; // Remove newline

    char log_entry[512];
    // Snprintf is generally safe; writing to file
    int len = snprintf(log_entry, sizeof(log_entry), "[%s] %s\n", timestamp, message);
    
    if (len > 0) {
        if (write(fd, log_entry, len) == -1) {
            perror("Error writing to syslog.txt");
        }
    }
    
    close(fd);
}

// Robust integer input
int get_input_int(const char *prompt) {
    char buffer[64];
    long val;
    char *endptr;

    printf("%s", prompt);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return -1;
    }

    buffer[strcspn(buffer, "\n")] = 0; // Remove newline

    errno = 0;
    val = strtol(buffer, &endptr, 10);

    // Check for errors: conversion failed, out of range, or extra chars
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || 
        (errno != 0 && val == 0)) {
        return -1;
    }
    if (endptr == buffer) { // No digits found
        return -1;
    }
    // Allow trailing whitespace but not other garbage? 
    // strict check: if (*endptr != '\0') return -1; 
    // But simple version is fine.

    return (int)val;
}

void clear_screen() {
    // Only clear if output is to a terminal
    if (isatty(STDOUT_FILENO)) {
        printf("\033[H\033[J");
    }
}

// Helper to read /proc/stat using system calls
void read_cpu_stats(CPUStats *stats) {
    int fd = open("/proc/stat", O_RDONLY);
    if (fd == -1) {
        perror("Error opening /proc/stat");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        // Parse the first line
        if (sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &stats->user, &stats->nice, &stats->system,
               &stats->idle, &stats->iowait, &stats->irq,
               &stats->softirq, &stats->steal) != 8) {
             // Handle parse error quietly or log
        }
    } else {
        perror("Error reading /proc/stat");
    }
    close(fd);
}

void getCPUUsage() {
    CPUStats stat1, stat2;
    printf("\n--- CPU Usage ---\n");
    printf("Measuring CPU usage (sampling 1 second)...\n");

    read_cpu_stats(&stat1);
    sleep(1);
    read_cpu_stats(&stat2);

    unsigned long long total1 = stat1.user + stat1.nice + stat1.system + stat1.idle + 
                                stat1.iowait + stat1.irq + stat1.softirq + stat1.steal;
    unsigned long long total2 = stat2.user + stat2.nice + stat2.system + stat2.idle + 
                                stat2.iowait + stat2.irq + stat2.softirq + stat2.steal;

    unsigned long long idle1 = stat1.idle + stat1.iowait;
    unsigned long long idle2 = stat2.idle + stat2.iowait;

    unsigned long long total_delta = total2 - total1;
    unsigned long long idle_delta = idle2 - idle1;

    double cpu_usage = 0.0;
    if (total_delta > 0) {
        cpu_usage = (double)(total_delta - idle_delta) / total_delta * 100.0;
    }

    printf("CPU Usage: %.2f%%\n", cpu_usage);
    
    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "CPU Usage checked: %.2f%%", cpu_usage);
    log_message(log_buf);
}

void getMemoryUsage() {
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd == -1) {
        perror("Error opening /proc/meminfo");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) {
        perror("Error reading /proc/meminfo");
        return;
    }
    buffer[bytes_read] = '\0';

    unsigned long long total_mem = 0;
    unsigned long long available_mem = 0;
    
    // Parse manually or using sscanf on lines
    char *line = strtok(buffer, "\n");
    while (line != NULL) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %llu kB", &total_mem);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %llu kB", &available_mem);
        }
        line = strtok(NULL, "\n");
    }

    printf("\n--- Memory Usage ---\n");
    if (total_mem > 0) {
        unsigned long long used_mem = total_mem - available_mem;
        double used_percent = (double)used_mem / total_mem * 100.0;
        printf("Total Memory: %llu MB\n", total_mem / 1024);
        printf("Used Memory:  %llu MB (%.2f%%)\n", used_mem / 1024, used_percent);
        printf("Free Memory:  %llu MB\n", available_mem / 1024);
        
        char log_buf[128];
        snprintf(log_buf, sizeof(log_buf), "Memory checked: Used %llu MB (%.2f%%)", used_mem / 1024, used_percent);
        log_message(log_buf);
    } else {
        printf("Could not read memory info.\n");
    }
}

int compare_processes(const void *a, const void *b) {
    ProcessInfo *pa = (ProcessInfo *)a;
    ProcessInfo *pb = (ProcessInfo *)b;
    // Sort descending by CPU time
    if (pb->cpu_time > pa->cpu_time) return 1;
    if (pb->cpu_time < pa->cpu_time) return -1;
    return 0;
}

void listTopProcesses() {
    DIR *dir;
    struct dirent *entry;
    ProcessInfo processes[1024]; 
    int count = 0;

    dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir /proc");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(*entry->d_name)) continue;

        int pid = atoi(entry->d_name);
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);

        int fd = open(path, O_RDONLY);
        if (fd != -1) {
            char buffer[512];
            ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
            close(fd);

            if (bytes > 0) {
                buffer[bytes] = '\0';
                
                // Parse /proc/[pid]/stat
                // Format: pid (comm) state ppid ... utime stime ...
                // The comm field is in parentheses and can contain spaces.
                
                char *close_paren = strrchr(buffer, ')');
                if (close_paren) {
                    char *name_start = strchr(buffer, '(');
                    if (name_start) {
                        // Extract name
                        size_t name_len = close_paren - name_start - 1;
                        if (name_len >= sizeof(processes[count].name)) name_len = sizeof(processes[count].name) - 1;
                        strncpy(processes[count].name, name_start + 1, name_len);
                        processes[count].name[name_len] = '\0';
                        
                        // Parse stats after ')'
                        unsigned long long utime = 0, stime = 0;
                        // The fields after ) are: state, ppid, pgrp, session, tty_nr, tpgid, flags, minflt, cminflt, majflt, cmajflt, utime, stime
                        // That is 13 fields to skip to get to utime (14th) and stime (15th)
                        // sscanf format string with skipped fields using *
                        
                        // Easier: use sscanf on the substring starting after close_paren
                        // Field 3 (state) is first char after space
                        // 3  4  5  6  7  8  9  10 11 12 13 14 15
                        // %c %d %d %d %d %d %u %u %u %u %u %llu %llu
                        
                        char state;
                        int ppid, pgrp, session, tty_nr, tpgid;
                        unsigned int flags;
                        unsigned long minflt, cminflt, majflt, cmajflt;
                        
                        sscanf(close_paren + 2, "%c %d %d %d %d %d %u %lu %lu %lu %lu %llu %llu",
                               &state, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,
                               &minflt, &cminflt, &majflt, &cmajflt,
                               &utime, &stime);

                        processes[count].pid = pid;
                        processes[count].cpu_time = utime + stime;
                        count++;
                        if (count >= 1024) break;
                    }
                }
            }
        }
    }
    closedir(dir);

    qsort(processes, count, sizeof(ProcessInfo), compare_processes);

    printf("\n--- Top 5 Processes (by Accumulated CPU Time) ---\n");
    printf("%-8s %-20s %-15s\n", "PID", "Name", "CPU Time (ticks)");
    for (int i = 0; i < 5 && i < count; i++) {
        printf("%-8d %-20s %llu\n", processes[i].pid, processes[i].name, processes[i].cpu_time);
    }
    log_message("Checked Top 5 Processes.");
}

void continuousMonitor(int interval) {
    printf("\nStarting Continuous Monitoring... (Press Ctrl+C to stop)\n");
    log_message("Started Continuous Monitoring.");
    
    // Set up signal handler specifically for this loop if needed, 
    // but global handler works too.
    
    while (keep_running) {
        clear_screen();
        printf("=============================================\n");
        printf("   SysMonitor++ - Continuous Monitoring      \n");
        printf("   Refresh Interval: %d seconds              \n", interval);
        printf("   (Press Ctrl+C to stop)                    \n");
        printf("=============================================\n");
        
        getCPUUsage();
        getMemoryUsage();
        listTopProcesses();
        
        printf("\nRefreshing in %d seconds...\n", interval);
        sleep(interval);
    }
    printf("\nContinuous monitoring stopped.\n");
}

void displayMenu() {
    clear_screen();
    printf("========================================\n");
    printf("       SysMonitor++ - System Monitor    \n");
    printf("========================================\n");
    printf("1. CPU Usage\n");
    printf("2. Memory Usage\n");
    printf("3. Top 5 Processes (CPU)\n");
    printf("4. Continuous Monitoring\n");
    printf("5. Exit\n");
    printf("========================================\n");
}

int main(int argc, char *argv[]) {
    // Setup Signal Handling
    struct sigaction sa;
    sa.sa_handler = handleSignal;
    sa.sa_flags = 0; // or SA_RESTART if we want to restart syscalls, but here we want to interrupt sleep
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting up signal handler");
        return 1;
    }

    // Check for Command Line Arguments
    if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        char *endptr;
        long val = strtol(argv[2], &endptr, 10);
        if (*endptr == '\0' && val > 0 && val <= 3600) {
            continuousMonitor((int)val);
            log_message("System Monitor terminated (Continuous Mode).");
            return 0;
        } else {
            fprintf(stderr, "Invalid interval. Please provide a value between 1 and 3600.\n");
            return 1;
        }
    }

    log_message("System Monitor started.");

    while (keep_running) {
        displayMenu();
        int choice = get_input_int("Enter your choice: ");
        
        if (choice == -1) {
            printf("Invalid input. Please enter a number.\n");
            sleep(1);
            continue;
        }

        switch (choice) {
            case 1:
                getCPUUsage();
                break;
            case 2:
                getMemoryUsage();
                break;
            case 3:
                listTopProcesses();
                break;
            case 4: {
                int interval = get_input_int("Enter refresh interval (seconds): ");
                if (interval > 0) {
                    continuousMonitor(interval);
                } else {
                    printf("Invalid interval.\n");
                }
                break;
            }
            case 5:
                printf("Exiting SysMonitor++...\n");
                log_message("System Monitor exited by user.");
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
        
        if (keep_running) {
            printf("\nPress Enter to return to menu...");
            while (getchar() != '\n'); // Wait for enter
        }
    }
    
    log_message("System Monitor terminated.");
    return 0;
}
