#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define LV_COUNT 4
#define LOG_FILE "/var/log/lvm_monitor.log"

typedef struct {
    char lv_name[32];
    char mount_point[64];
} LV;

// Shared variables
volatile sig_atomic_t extend_requested = 0;
char current_lv[32] = "";
int pipe_fd[2];

LV lvs[LV_COUNT] = {
    {"logic1", "/point1"},
    {"logic2", "/point2"},
    {"logic3", "/point3"},
    {"logic4", "/point4"}
};

void log_message(const char *msg) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str)-1] = '\0'; // Remove newline
        fprintf(log, "[%s] %s\n", time_str, msg);
        fclose(log);
    }
    printf("%s\n", msg);
}

/* -----------------------------------------------------
                  EXTENDER PROCESS (AUTO)
   ----------------------------------------------------- */
void extender_signal_handler(int sig) {
    extend_requested = 1;
}

void extender_process() {
    signal(SIGUSR1, extender_signal_handler);
    char log_msg[256];

    while (1) {
        if (extend_requested) {
            extend_requested = 0;
            
            char lv_name[32];
            read(pipe_fd[0], lv_name, sizeof(lv_name));

            snprintf(log_msg, sizeof(log_msg), 
                     "[EXTENDER] AUTO-EXTENDING LV: %s", lv_name);
            log_message(log_msg);

            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                     "lvextend -L +1G /dev/proj/%s 2>&1 && resize2fs /dev/proj/%s 2>&1",
                     lv_name, lv_name);

            FILE *fp = popen(cmd, "r");
            if (fp) {
                char output[256];
                while (fgets(output, sizeof(output), fp)) {
                    output[strcspn(output, "\n")] = 0;
                    snprintf(log_msg, sizeof(log_msg), 
                             "[EXTENDER] %s", output);
                    log_message(log_msg);
                }
                pclose(fp);
            }

            snprintf(log_msg, sizeof(log_msg), 
                     "[EXTENDER] Extension completed for %s", lv_name);
            log_message(log_msg);
        }
        
        pause();
    }
}


/* -----------------------------------------------------
                  SUPERVISOR PROCESS (AUTO)
   ----------------------------------------------------- */
void supervisor_process(pid_t extender_pid) {
    char log_msg[256];
    log_message("[SUPERVISOR] Starting automatic monitoring...");

    while (1) {
        for (int i = 0; i < LV_COUNT; i++) {
            char cmd[256], result[16];
            FILE *fp;

            snprintf(cmd, sizeof(cmd),
                     "df --output=pcent %s 2>/dev/null | tail -1",
                     lvs[i].mount_point);

            fp = popen(cmd, "r");
            if (!fp) continue;

            fgets(result, sizeof(result), fp);
            pclose(fp);

            int usage = atoi(result);

            if (usage >= 90) {
                snprintf(log_msg, sizeof(log_msg),
                         "[SUPERVISOR] WARNING: %s (%s) at %d%% - Triggering auto-extension",
                         lvs[i].lv_name, lvs[i].mount_point, usage);
                log_message(log_msg);

                write(pipe_fd[1], lvs[i].lv_name, sizeof(lvs[i].lv_name));
                kill(extender_pid, SIGUSR1);
                
                sleep(10); // Wait for extension to complete
            }
        }

        sleep(5); // Check every 5 seconds
    }
}


/* -----------------------------------------------------
              WRITER PROCESS (AUTOMATED)
   ----------------------------------------------------- */
void writer_process() {
    char log_msg[256];
    log_message("[WRITER] Starting automated file generation...");

    srand(time(NULL) + getpid());
    int file_counter = 0;

    while (1) {
        // Select random mount point
        int lv_index = rand() % LV_COUNT;
        
        char filepath[256];
        snprintf(filepath, sizeof(filepath), 
                 "%s/autofile_%d_%ld.txt", 
                 lvs[lv_index].mount_point, 
                 file_counter++,
                 time(NULL));

        // Generate file with random size (5-50 MB)
        int size_mb = 5 + (rand() % 46);
        
        snprintf(log_msg, sizeof(log_msg),
                 "[WRITER] Creating %dMB file: %s", size_mb, filepath);
        log_message(log_msg);

        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "dd if=/dev/urandom of=%s bs=1M count=%d 2>/dev/null",
                 filepath, size_mb);
        
        system(cmd);

        snprintf(log_msg, sizeof(log_msg),
                 "[WRITER] File created successfully");
        log_message(log_msg);

        // Wait 10-30 seconds before next file
        int wait_time = 10 + (rand() % 21);
        sleep(wait_time);
    }
}


/* -----------------------------------------------------
                    PARENT PROCESS
   ----------------------------------------------------- */
void print_status() {
    printf("\n========================================\n");
    printf("   LVM AUTO-MONITOR STATUS\n");
    printf("========================================\n");
    system("df -h /point1 /point2 /point3 /point4 2>/dev/null");
    printf("========================================\n");
    printf("Log file: %s\n", LOG_FILE);
    printf("Press Ctrl+C to stop monitoring\n");
    printf("========================================\n\n");
}

void cleanup(int sig) {
    log_message("[PARENT] Shutting down monitoring system...");
    printf("\n[PARENT] Cleaning up and exiting...\n");
    exit(0);
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    pipe(pipe_fd);

    log_message("=== LVM AUTO-MONITOR SYSTEM STARTED ===");

    pid_t extender_pid = fork();
    if (extender_pid == 0) {
        extender_process();
        exit(0);
    }

    pid_t supervisor_pid = fork();
    if (supervisor_pid == 0) {
        supervisor_process(extender_pid);
        exit(0);
    }

    pid_t writer_pid = fork();
    if (writer_pid == 0) {
        writer_process();
        exit(0);
    }

    printf("[PARENT] System started. All processes automated.\n");
    printf("[PARENT] Writer: Auto-generating files\n");
    printf("[PARENT] Supervisor: Monitoring usage every 5 seconds\n");
    printf("[PARENT] Extender: Auto-extending volumes when needed\n\n");

    // Display status every 30 seconds
    while (1) {
        print_status();
        sleep(30);
    }

    return 0;
}
