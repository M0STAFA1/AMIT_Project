#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

// Global variable to control the program flow
volatile sig_atomic_t keep_running = 1;

// Function to handle Ctrl+C (SIGINT) and stop the loop
void handle_sigint(int sig) {
    keep_running = 0;
}

// Function to get the terminal size
void get_terminal_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
        exit(1);
    }
    *rows = ws.ws_row;
    *cols = ws.ws_col;
}

// Function to get the username by UID
const char* get_username_by_uid(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    } else {
        return "Unknown";
    }
}

// Function to get total system memory from /proc/meminfo
unsigned long get_total_memory() {
    FILE *file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Failed to open /proc/meminfo");
        return 0;
    }

    unsigned long total_mem = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "MemTotal:", 9) == 0) {
            sscanf(buffer, "MemTotal: %lu kB", &total_mem);
            break;
        }
    }

    fclose(file);
    return total_mem;
}

// Function to get the memory usage of a process from /proc/[pid]/status
unsigned long get_process_memory(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    unsigned long mem_usage = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "VmRSS:", 6) == 0) {
            sscanf(buffer, "VmRSS: %lu kB", &mem_usage);
            break;
        }
    }

    fclose(file);
    return mem_usage;
}

// Function to get the UID of a process from /proc/[pid]/status
uid_t get_process_uid(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return (uid_t) -1;  // Return -1 if the file cannot be opened
    }

    uid_t uid = (uid_t) -1;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "Uid:", 4) == 0) {
            sscanf(buffer, "Uid:\t%u", &uid);  // Extract the UID
            break;
        }
    }

    fclose(file);
    return uid;
}

// Function to get the total CPU time (user + system) for a process from /proc/[pid]/stat
unsigned long get_process_time(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    unsigned long utime, stime;
    char buffer[256];
    fgets(buffer, sizeof(buffer), file);  // Read the whole line

    // Extract the utime (user time) and stime (system time) fields
    sscanf(buffer, "%*d %*s %*c %*d %*d %*d %*d %*d %*d %*d %lu %lu", &utime, &stime);

    fclose(file);

    // Return the total time (user + system time) in clock ticks
    return utime + stime;
}

// Function to print the table header
void print_table_header() {
    printf("\n+--------------+------------------------+-----------------+---------------+------------+------------------------+---------------------------+\n");
    printf("| %-12s | %-22s | %-15s | %-13s | %-10s | %-22s | %-25s |\n", 
           "PID", "User", "Priority", "CPU Usage", "Mem(kB)", "Time", "Command");
    printf("+--------------+------------------------+-----------------+---------------+------------+------------------------+---------------------------+\n");
}

// Function to format the time (limit to two decimal places and add a unit)
void format_time(double cpu_time_seconds, char *formatted_time) {
    if (cpu_time_seconds < 1.0) {
        // If time is less than 1 second, display in milliseconds
        snprintf(formatted_time, 20, "%.2f ms", cpu_time_seconds * 1000);
    } else {
        // If time is 1 second or more, display in seconds
        snprintf(formatted_time, 20, "%.2f s", cpu_time_seconds);
    }
}

// Function to print the table rows from the file
void print_file_with_header(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    unsigned long total_mem = get_total_memory();  // Get total system memory in kB
    char line[256];

    // Terminal size variables
    int rows, cols;
    get_terminal_size(&rows, &cols);  // Get current terminal size

    int current_line = 0;  // Keep track of how many lines we've printed
    int lines_per_page = rows - 5;  // Subtract 5 for the header and borders

    // Read each line from the file and print it
    while (fgets(line, sizeof(line), file)) {
        // Strip any trailing newline character
        line[strcspn(line, "\n")] = 0;

        // Split line by '|' to get the columns
        char *pid_str = strtok(line, "|");
        char *user = strtok(NULL, "|");
        char *priority = strtok(NULL, "|");
        char *cpu_usage = strtok(NULL, "|");
        char *mem_usage_str = strtok(NULL, "|");
        char *command = strtok(NULL, "|");

        pid_t pid = atoi(pid_str);
        
        // Get the UID from /proc/[pid]/status and fetch the corresponding username
        uid_t uid = get_process_uid(pid);
        const char *username = get_username_by_uid(uid);

        unsigned long mem_usage = get_process_memory(pid);  // Get memory usage in kB
        unsigned long cpu_time = get_process_time(pid);  // Get total CPU time (user + system)

        // Convert clock ticks to seconds (assuming 100 Hz clock)
        double cpu_time_seconds = (double)cpu_time / sysconf(_SC_CLK_TCK);

        // Format the time with unit and limitation
        char formatted_time[20];
        format_time(cpu_time_seconds, formatted_time);

        // Print the row in the formatted table
        printf("| %-10s | %-22s | %-13s | %-13s | %-10lu | %-22s | %-25s |\n", 
               pid_str, username, priority, cpu_usage, mem_usage, formatted_time, command);

        current_line++;

        // If we've printed enough lines, wait for the user to press Enter
        if (current_line >= lines_per_page) {
            printf("\nPress Enter to continue or Ctrl+C to exit...\n");
            current_line = 0;

            // Wait for user to press Enter
            while (1) {
                char ch = getchar();
                if (ch == '\n' || ch == EOF) {
                    break;
                }
            }

            // Exit if Ctrl+C is pressed
            if (!keep_running) {
                break;
            } else {
                print_table_header();
            }
        }
    }

    fclose(file);
}

int main() {
    // Set up the signal handler for Ctrl+C
    signal(SIGINT, handle_sigint);

    // Path to the /proc/proc_info file
    const char *filename = "/proc/proc_info";  // Your file with process information
    
    print_table_header();
    
    // Print the file contents with the header
    print_file_with_header(filename);

    return 0;
}
