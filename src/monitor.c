#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

/* Helper function to read /proc/stat and extract total and idle jiffies */
static void get_cpu_times(unsigned long long *idle, unsigned long long *total) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        char cpu_label[10];
        unsigned long long user, nice, system, idle_time, iowait, irq, softirq, steal, guest, guest_nice;
        
        int scanned = sscanf(buffer, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
               cpu_label, &user, &nice, &system, &idle_time, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
        
        if (scanned >= 5 && strcmp(cpu_label, "cpu") == 0) {
            *idle = idle_time;
            *total = user + nice + system + idle_time + iowait + irq + softirq;
            if (scanned >= 9) *total += steal;
            if (scanned >= 10) *total += guest;
            if (scanned >= 11) *total += guest_nice;
        }
    }
    fclose(fp);
}

void monitor_init(monitor_t *mon, circ_buff_t *buffer, pthread_mutex_t *counters_mutex,
                  unsigned int *commit_count, unsigned int *identity_count,
                  unsigned int *account_count, unsigned int *info_count) {
    mon->buffer = buffer;
    mon->counters_mutex = counters_mutex;
    mon->commit_count = commit_count;
    mon->identity_count = identity_count;
    mon->account_count = account_count;
    mon->info_count = info_count;
    mon->running = 1;
}

void* monitor_thread_func(void *arg) {
    monitor_t *mon = (monitor_t *)arg;
    
    // Open the log file in append mode
    FILE *log_file = fopen("metrics_log.txt", "a");
    if (!log_file) {
        perror("Failed to open metrics_log.txt");
        return NULL;
    }

    // Write CSV Header if the file is empty (optional but good practice)
    fseek(log_file, 0, SEEK_END);
    if (ftell(log_file) == 0) {
        fprintf(log_file, "Seconds,Nanoseconds,Commit_Count,Identity_Count,Account_Count,Info_Count,Buffer_Occupancy_Pct,CPU_Pct\n");
        fflush(log_file);
    }

    // Initialize CPU tracking state
    unsigned long long prev_idle = 0, prev_total = 0;
    get_cpu_times(&prev_idle, &prev_total);

    // Set up the absolute timer for the first sleep cycle
    struct timespec next_wakeup;
    clock_gettime(CLOCK_MONOTONIC, &next_wakeup);

    while (mon->running) {
        // Advance the wakeup deadline by exactly 1 second to avoid clock drift
        next_wakeup.tv_sec += 1;

        // Sleep until the exact deadline
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wakeup, NULL);

        if (!mon->running) break;

        // 1. Get exact current time for logging
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        // 2. Calculate CPU usage percentage
        unsigned long long current_idle = 0, current_total = 0;
        get_cpu_times(&current_idle, &current_total);
        
        unsigned long long idle_delta = current_idle - prev_idle;
        unsigned long long total_delta = current_total - prev_total;
        
        double cpu_pct = 0.0;
        if (total_delta > 0) {
            cpu_pct = (1.0 - ((double)idle_delta / total_delta)) * 100.0;
        }
        
        prev_idle = current_idle;
        prev_total = current_total;

        // 3. Lock mutex, copy data, reset counters, and calculate buffer occupancy
        pthread_mutex_lock(mon->counters_mutex);
        
        unsigned int commits = *(mon->commit_count);
        unsigned int identities = *(mon->identity_count);
        unsigned int accounts = *(mon->account_count);
        unsigned int infos = *(mon->info_count);
        
        *(mon->commit_count) = 0;
        *(mon->identity_count) = 0;
        *(mon->account_count) = 0;
        *(mon->info_count) = 0;
        
        double buffer_pct = circ_buff_get_occupancy_pct(mon->buffer);
        
        pthread_mutex_unlock(mon->counters_mutex);

        // 4. Append metrics to log file
        fprintf(log_file, "%ld,%ld,%u,%u,%u,%u,%.2f,%.2f\n",
                (long)ts.tv_sec,
                (long)ts.tv_nsec,
                commits,
                identities,
                accounts,
                infos,
                buffer_pct,
                cpu_pct);
        
        fflush(log_file); // Ensure data is written to disk immediately
    }

    fclose(log_file);
    return NULL;
}