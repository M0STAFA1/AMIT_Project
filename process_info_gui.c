#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <glib.h>
#include <execinfo.h>  // Include this header for backtrace functions

// Declare global variables
GtkTreeStore *store;
GHashTable *seen_pids;  // To track seen PIDs

// Function prototypes
void populate_treeview(GtkTreeStore *store, GtkTreeIter *parent);
void refresh_data(GtkWidget *widget, gpointer data);
void collapse_treeview(GtkWidget *widget, gpointer data);
void expand_all(GtkWidget *widget, gpointer data);
void kill_process(GtkWidget *widget, gpointer data);
unsigned long get_process_memory(pid_t pid);
const char* get_username_by_uid(uid_t uid);
unsigned long get_process_cpu_time(pid_t pid);
void handle_segfault(int sig, siginfo_t *info, void *context);

// Signal handler to capture segmentation faults
void handle_segfault(int sig, siginfo_t *info, void *context) {
    g_print("Segmentation fault caught! Signal: %d\n", sig);
    g_print("Stack trace:\n");

    // Print backtrace
    void *array[10];
    size_t size = backtrace(array, 10);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// Function to get process memory usage
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

// Function to get process CPU time
unsigned long get_process_cpu_time(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    unsigned long utime, stime;
    char buffer[256];
    fgets(buffer, sizeof(buffer), file);

    sscanf(buffer, "%*d %*s %*c %*d %*d %*d %*d %*d %*d %*d %lu %lu", &utime, &stime);

    fclose(file);
    return utime + stime;
}

// Function to get the username of a process by UID
const char* get_username_by_uid(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    } else {
        return "Unknown";
    }
}

// Helper function to get the parent of a process
GtkTreeIter get_parent_iter(GtkTreeStore *store, pid_t ppid, GtkTreeIter *parent_iter) {
    GtkTreeIter iter;
    gboolean found = FALSE;

    // Iterate through the tree to find the parent with matching PPID
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            pid_t pid;
            gtk_tree_model_get(model, &iter, 0, &pid, -1);
            if (pid == ppid) {
                *parent_iter = iter;
                found = TRUE;
                break;
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    if (found) {
        return *parent_iter;
    } else {
        // Return an empty iter if no parent is found
        gtk_tree_store_append(store, &iter, NULL);
        return iter;
    }
}

// Populate treeview with process information
void populate_treeview(GtkTreeStore *store, GtkTreeIter *parent) {
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            pid_t pid = atoi(entry->d_name);

            // Skip already processed PIDs
            if (g_hash_table_contains(seen_pids, &pid)) {
                continue;
            }

            // Add the PID to the seen_pids table
            g_hash_table_insert(seen_pids, g_malloc(sizeof(int)), &pid);

            char path[256];
            snprintf(path, sizeof(path), "/proc/%d/status", pid);

            FILE *file = fopen(path, "r");
            if (!file) {
                continue;
            }

            char comm[256] = {0};
            uid_t uid = 0;
            unsigned long mem_usage = 0;
            unsigned long cpu_time = 0;
            pid_t ppid = 0;

            char buffer[256];
            while (fgets(buffer, sizeof(buffer), file)) {
                if (strncmp(buffer, "Name:", 5) == 0) {
                    sscanf(buffer, "Name:\t%s", comm);
                }
                if (strncmp(buffer, "Uid:", 4) == 0) {
                    sscanf(buffer, "Uid:\t%u", &uid);
                }
                if (strncmp(buffer, "PPid:", 5) == 0) {
                    sscanf(buffer, "PPid:\t%d", &ppid);
                }
            }
            fclose(file);

            mem_usage = get_process_memory(pid);
            cpu_time = get_process_cpu_time(pid);

            GtkTreeIter iter;
            // Find the parent first
            GtkTreeIter parent_iter;
            if (ppid > 0) {
                parent_iter = get_parent_iter(store, ppid, &parent_iter);
                gtk_tree_store_append(store, &iter, &parent_iter);  // Add as child
            } else {
                // For system processes with no parent (e.g., PID 1), append at the root level
                gtk_tree_store_append(store, &iter, parent);
            }

            gtk_tree_store_set(store, &iter,
                               0, pid,
                               1, get_username_by_uid(uid),
                               2, comm,
                               3, mem_usage,
                               4, cpu_time,
                               -1);
        }
    }

    closedir(dir);
}

// Refresh the data in the treeview
void refresh_data(GtkWidget *widget, gpointer data) {
    gtk_tree_store_clear(store);
    populate_treeview(store, NULL);
}

// Collapse all rows in the treeview
void collapse_treeview(GtkWidget *widget, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL(store);

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_view_collapse_row(GTK_TREE_VIEW(data), path);
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

// Expand all rows in the treeview
void expand_all(GtkWidget *widget, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL(store);

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_view_expand_row(GTK_TREE_VIEW(data), path, FALSE);
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

// Kill a selected process
void kill_process(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint pid;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &pid, -1);

        if (pid > 0) {
            int result = kill(pid, SIGKILL);
            if (result == 0) {
                g_print("Process %d killed successfully\n", pid);
            } else {
                g_print("Failed to kill process %d\n", pid);
            }
        }
    }
}

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Set GTK to use dark mode (based on the environment theme)
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-theme-name", "Adwaita-dark", NULL);

    // Set up signal handler for segmentation faults
    struct sigaction sa;
    sa.sa_sigaction = handle_segfault;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    seen_pids = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);  // Initialize hash table for seen PIDs

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Process Information");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Set an application icon (make sure you have a PNG file in your project directory)
    GdkPixbuf *icon = gdk_pixbuf_new_from_file("icon.png", NULL);
    gtk_window_set_icon(GTK_WINDOW(window), icon);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    store = gtk_tree_store_new(5, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("PID", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    column = gtk_tree_view_column_new_with_attributes("User", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    column = gtk_tree_view_column_new_with_attributes("Command", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    column = gtk_tree_view_column_new_with_attributes("Memory", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    column = gtk_tree_view_column_new_with_attributes("CPU Time", renderer, "text", 4, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), treeview);

    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh");
    GtkWidget *collapse_button = gtk_button_new_with_label("Collapse All");
    GtkWidget *expand_button = gtk_button_new_with_label("Expand All");
    GtkWidget *kill_button = gtk_button_new_with_label("Kill Process");

    g_signal_connect(refresh_button, "clicked", G_CALLBACK(refresh_data), NULL);
    g_signal_connect(collapse_button, "clicked", G_CALLBACK(collapse_treeview), treeview);
    g_signal_connect(expand_button, "clicked", G_CALLBACK(expand_all), treeview);
    g_signal_connect(kill_button, "clicked", G_CALLBACK(kill_process), treeview);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(button_box), refresh_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), collapse_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), expand_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), kill_button, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled_window, TRUE, TRUE, 0);

    populate_treeview(store, NULL);

    gtk_widget_show_all(window);

    gtk_main();

    g_hash_table_destroy(seen_pids);  // Clean up the hash table

    return 0;
}
