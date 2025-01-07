#include <linux/module.h>       // Provides macros and declarations for creating a module.
#include <linux/kernel.h>       //Contains basic kernel functions (e.g., printk() for logging).
#include <linux/init.h>         //Contains macros for initialization and cleanup functions (module_init(), module_exit()).
#include <linux/sched.h>      // For task_struct, which represents each running process in Linux.
#include <linux/fs.h>         // rovides the file system-related functions, like seq_file
#include <linux/proc_fs.h>    // Defines the functions for interacting with the /proc file system.
#include <linux/uidgid.h>     // Defines kuid_t, used to handle user IDs (UIDs).
#include <linux/seq_file.h>   // Defines functions like seq_printf for printing formatted output to a sequence file (used for /proc).

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A kernel module to display process info like user, priority, CPU usage");

#define PROC_NAME "proc_info"

// Function to get the username by UID (you might need to replace this with something more efficient)
const char* get_username_by_uid(kuid_t uid) {
    // In kernel space, there is no simple function like getpwuid() to get the username.
    // However, you can directly map the UID to "root" or use a placeholder like "Unknown" if needed.
    // UID 0 corresponds to "root" in Linux.
    if (uid.val == 0) {
        return "root";  // UID 0 is always root
    }
    
    return "Unknown";  // For simplicity, return "Unknown" for other UIDs
}

// Function to display the information about processes
static int proc_info_show(struct seq_file *m, void *v) {
    struct task_struct *task;
    unsigned long utime, stime;
    
    // Iterate through all tasks (processes)
    for_each_process(task) {
        // Get user info, priority, etc.
        const char *user = get_username_by_uid(task->real_cred->uid);
        int pr = task->prio;
        unsigned long mem_usage = task->mm ? task->mm->total_vm : 0;  // Use total_vm for memory usage
        
        utime = task->utime;  // User time
        stime = task->stime;  // System time

        unsigned long total_time = utime + stime;

        // Print the process info in a table format
        seq_printf(m, "| %-10d | %-20s | %-13d | %-8lu | %-10lu | %-20s |\n",
                   task->pid, user, pr, mem_usage, total_time, task->comm);
    }
    return 0;
}

static int proc_info_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_info_show, NULL);
}

static const struct proc_ops proc_info_fops = {
    .proc_open = proc_info_open, //Called when the file is opened
    .proc_read = seq_read, //Reads data from the file
    .proc_release = single_release, //Handles cleanup when the file is closed.
};

// Module initialization
static int __init proc_info_init(void) {
    // Create a /proc entry for our module
    proc_create(PROC_NAME, 0, NULL, &proc_info_fops);
    printk(KERN_INFO "Kernel Module for Process Info Loaded\n");
    return 0;
}

// Module cleanup
static void __exit proc_info_exit(void) {
    // Remove the /proc entry when the module is unloaded
    remove_proc_entry(PROC_NAME, NULL);
    printk(KERN_INFO "Kernel Module for Process Info Unloaded\n");
}

module_init(proc_info_init);
module_exit(proc_info_exit);
