#include <gtk/gtk.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h> // For reading directories (sysfs)
#include <limits.h> // For PATH_MAX

// This header file defines NETLINK_USER, MAX_GPUS_SUPPORTED,
// struct gpu_info_packet, struct all_gpus_info_packet, and MAX_PAYLOAD.
// It should be located in ../include/ relative to this C file.
#include "../include/gpu_proto.h"

// Global variables
GtkListStore *list_store;
GtkButton *scan_button;
GtkLabel *status_label;
int sock_fd = -1;
gboolean is_scanning = FALSE;
guint timer_id = 0;

#define UPDATE_INTERVAL_SECONDS 3
#define MAX_VALUE_LEN 256
#define PCI_IDS_PATH "/usr/share/hwdata/pci.ids" // Common path for pci.ids

// Enum for GtkTreeView columns, inspired by Activity Monitor style
enum {
    COLUMN_BUS = 0,
    COLUMN_SLOT,
    COLUMN_FUNCTION,
    COLUMN_VENDOR_ID,
    COLUMN_VENDOR_NAME,
    COLUMN_DEVICE_ID,
    COLUMN_DEVICE_NAME,
    COLUMN_UTILIZATION,     // Will attempt to show real data
    // COLUMN_TEMPERATURE,  // Removed
    COLUMN_LAST_UPDATED,
    NUM_DISPLAY_COLUMNS_ACTIVITY // Updated count
};

// --- PCI ID Lookup Functions (from pci.ids) ---
char *trim_whitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    return str;
}

char* get_vendor_name_from_pci_ids(unsigned short vendor_id) {
    FILE *fp;
    char line[512];
    char target_vid_str[10];
    char *name_found = NULL;

    if (vendor_id == 0x0000) return strdup("N/A");
    if (vendor_id == 0x1AF4) return strdup("Red Hat, Inc. (QEMU virtual)");

    fp = fopen(PCI_IDS_PATH, "r");
    if (fp == NULL) {
        name_found = (char*)malloc(MAX_VALUE_LEN);
        if (name_found) snprintf(name_found, MAX_VALUE_LEN, "0x%04X (pci.ids error)", vendor_id);
        else name_found = strdup("File Error");
        return name_found;
    }
    snprintf(target_vid_str, sizeof(target_vid_str), "%04x", vendor_id);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == 'C') continue;
        if (line[0] != '\t') {
            char current_vid_str[5] = {0};
            strncpy(current_vid_str, line, 4);
            if (strcmp(current_vid_str, target_vid_str) == 0) {
                char *name_ptr = line + 4;
                while (*name_ptr && isspace((unsigned char)*name_ptr)) name_ptr++;
                name_found = strdup(trim_whitespace(name_ptr));
                break;
            }
        }
    }
    fclose(fp);
    if (!name_found) {
        name_found = (char*)malloc(MAX_VALUE_LEN);
        if (name_found) snprintf(name_found, MAX_VALUE_LEN, "Unknown Vendor (0x%04X)", vendor_id);
        else name_found = strdup("Unknown");
    }
    return name_found;
}

char* get_device_name_from_pci_ids(unsigned short vendor_id, unsigned short device_id) {
    FILE *fp;
    char line[512];
    char target_vendor_id_str[10], target_device_id_str[10];
    char *name_found = NULL;
    int in_target_vendor_section = 0;

    if (vendor_id == 0x0000 && device_id == 0x0000) return strdup("N/A");
    if (vendor_id == 0x1AF4 && device_id == 0x1050) return strdup("QXL paravirtual graphic card");

    fp = fopen(PCI_IDS_PATH, "r");
    if (fp == NULL) {
        name_found = (char*)malloc(MAX_VALUE_LEN);
        if (name_found) snprintf(name_found, MAX_VALUE_LEN, "0x%04X (pci.ids error)", device_id);
        else name_found = strdup("File Error");
        return name_found;
    }
    snprintf(target_vendor_id_str, sizeof(target_vendor_id_str), "%04x", vendor_id);
    snprintf(target_device_id_str, sizeof(target_device_id_str), "%04x", device_id);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (line[0] != '\t' && line[0] != 'C') {
            if (in_target_vendor_section) break;
            char current_vid_str[5] = {0};
            strncpy(current_vid_str, line, 4);
            if (strcmp(current_vid_str, target_vendor_id_str) == 0) in_target_vendor_section = 1;
        } else if (in_target_vendor_section && line[0] == '\t' && line[1] != '\t') {
            char current_did_str[5] = {0};
            strncpy(current_did_str, line + 1, 4);
            if (strcmp(current_did_str, target_device_id_str) == 0) {
                char *name_ptr = line + 1 + 4;
                while (*name_ptr && isspace((unsigned char)*name_ptr)) name_ptr++;
                name_found = strdup(trim_whitespace(name_ptr));
                break;
            }
        } else if (line[0] != '\t' && in_target_vendor_section) {
            break;
        }
    }
    fclose(fp);
    if (!name_found) {
        name_found = (char*)malloc(MAX_VALUE_LEN);
        if (name_found) snprintf(name_found, MAX_VALUE_LEN, "Unknown Device (0x%04X)", device_id);
        else name_found = strdup("Unknown");
    }
    return name_found;
}

// --- Sysfs GPU Utilization Reading Function (Best Effort) ---
// Returns utilization as a string (e.g., "75 %") or "N/A". Caller must free the returned string.
char* get_gpu_utilization_from_sysfs(unsigned char bus, unsigned char slot, unsigned char func) {
    char pci_domain_bus_slot_func[32]; // To store "0000:bus:slot.func"
    char drm_card_path_base[PATH_MAX];
    char drm_device_link_target[PATH_MAX];
    char sysfs_util_path[PATH_MAX];
    char util_value_str[32]; // To read utilization value from file
    FILE *fp_util;
    DIR *drm_dir;
    struct dirent *drm_entry;
    char *util_display_str = NULL;

    // Construct the PCI address string (Domain is usually 0000 for primary bus)
    snprintf(pci_domain_bus_slot_func, sizeof(pci_domain_bus_slot_func), "0000:%02x:%02x.%x", bus, slot, func);
    printf("GUI_DEBUG: Attempting to find sysfs utilization for PCI device: %s\n", pci_domain_bus_slot_func);

    drm_dir = opendir("/sys/class/drm/");
    if (!drm_dir) {
        perror("GUI_WARNING: Could not open /sys/class/drm/");
        return strdup("N/A (DRM Error)");
    }

    while ((drm_entry = readdir(drm_dir)) != NULL) {
        // Look for cardX entries (e.g., card0, card1)
        if (strncmp(drm_entry->d_name, "card", 4) == 0 && isdigit(drm_entry->d_name[4])) {
            snprintf(drm_card_path_base, sizeof(drm_card_path_base), "/sys/class/drm/%s/device", drm_entry->d_name);

            ssize_t len = readlink(drm_card_path_base, drm_device_link_target, sizeof(drm_device_link_target) - 1);
            if (len != -1) {
                drm_device_link_target[len] = '\0';
                // The link target is usually relative, e.g., ../../../0000:01:00.0
                // We need the basename of this link target.
                char *pci_basename_from_link = strrchr(drm_device_link_target, '/');
                if (pci_basename_from_link) {
                    pci_basename_from_link++; // Move past the '/'
                    if (strcmp(pci_basename_from_link, pci_domain_bus_slot_func) == 0) {
                        printf("GUI_DEBUG: Found matching DRM device %s for PCI %s\n", drm_entry->d_name, pci_domain_bus_slot_func);

                        // Try common sysfs path for AMD GPU utilization
                        snprintf(sysfs_util_path, sizeof(sysfs_util_path), "/sys/class/drm/%s/device/gpu_busy_percent", drm_entry->d_name);
                        fp_util = fopen(sysfs_util_path, "r");
                        if (fp_util) {
                            if (fgets(util_value_str, sizeof(util_value_str), fp_util)) {
                                int util_val = atoi(trim_whitespace(util_value_str));
                                util_display_str = (char*)malloc(MAX_VALUE_LEN);
                                if(util_display_str) snprintf(util_display_str, MAX_VALUE_LEN, "%d %%", util_val);
                            }
                            fclose(fp_util);
                            if (util_display_str) break; // Found it
                        }

                        // Add more vendor-specific paths here if known
                        // e.g., for Intel, it's more complex, often requiring calculation from busy/idle counters.
                        // For NVIDIA (Nouveau), sysfs entries might be limited.

                        if (util_display_str) break; // Break from drm_entry loop if found
                    }
                }
            }
        }
    }
    closedir(drm_dir);

    if (!util_display_str) { // If no specific path found or read failed
        util_display_str = strdup("N/A");
    }
    return util_display_str;
}


// --- GtkTreeView Helper Functions ---
void clear_list_store(void) { if (!list_store) return; gtk_list_store_clear(list_store); }
void update_status_label(const char *message) { if (!status_label) return; gtk_label_set_text(status_label, message); }

// --- Netlink Communication and Display Update ---
void perform_scan_and_update_display(void) {
    char recv_buf[MAX_PAYLOAD];
    struct nlmsghdr *nlh_recv;
    struct sockaddr_nl src_addr; socklen_t addr_len = sizeof(src_addr); int len;
    char status_msg_buf[MAX_VALUE_LEN];

    struct nlmsghdr *nlh_send; struct sockaddr_nl dest_addr; struct iovec iov;
    struct msghdr msg_send_hdr; char send_buf[NLMSG_SPACE(MAX_PAYLOAD)];
    char user_msg[] = "GET_ALL_GPUS_INFO"; int ret;

    update_status_label("Scanning...");
    if (sock_fd < 0) { /* ... (error handling) ... */
        clear_list_store(); update_status_label("Error: Netlink socket not initialized.");
        if (is_scanning && timer_id != 0) { g_source_remove(timer_id); timer_id = 0; is_scanning = FALSE; gtk_button_set_label(scan_button, "Start Scan");}
        return;
    }

    memset(send_buf, 0, sizeof(send_buf)); memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK; dest_addr.nl_pid = 0; dest_addr.nl_groups = 0;
    nlh_send = (struct nlmsghdr *)send_buf;
    nlh_send->nlmsg_len = NLMSG_SPACE(strlen(user_msg) + 1);
    nlh_send->nlmsg_pid = getpid(); nlh_send->nlmsg_flags = 0;
    memcpy(NLMSG_DATA(nlh_send), user_msg, strlen(user_msg) + 1);
    iov.iov_base = (void *)nlh_send; iov.iov_len = nlh_send->nlmsg_len;
    memset(&msg_send_hdr, 0, sizeof(msg_send_hdr));
    msg_send_hdr.msg_name = (void *)&dest_addr; msg_send_hdr.msg_namelen = sizeof(dest_addr);
    msg_send_hdr.msg_iov = &iov; msg_send_hdr.msg_iovlen = 1;

    ret = sendmsg(sock_fd, &msg_send_hdr, 0);
    if (ret < 0) { /* ... (error handling) ... */
        snprintf(status_msg_buf, sizeof(status_msg_buf), "Error: sendmsg failed: %s", strerror(errno));
        clear_list_store(); update_status_label(status_msg_buf); perror("sendmsg"); return;
    }

    nlh_recv = (struct nlmsghdr *)recv_buf;
    len = recvfrom(sock_fd, nlh_recv, MAX_PAYLOAD, 0, (struct sockaddr *)&src_addr, &addr_len);
    clear_list_store();

    if (len < 0) { /* ... (error handling) ... */
        snprintf(status_msg_buf, sizeof(status_msg_buf), "Error: recvfrom failed: %s", strerror(errno));
        update_status_label(status_msg_buf); perror("recvfrom"); return;
    }
    if (len == 0) { update_status_label("Info: No data from kernel module."); return; }

    if (nlh_recv->nlmsg_len < NLMSG_HDRLEN || (unsigned int)len < nlh_recv->nlmsg_len ||
        (unsigned int)len < (NLMSG_HDRLEN + offsetof(struct all_gpus_info_packet, gpus))) {
        update_status_label("Error: Received corrupted/incomplete multi-GPU message header."); return;
    }

    struct all_gpus_info_packet *all_gpus = (struct all_gpus_info_packet *)NLMSG_DATA(nlh_recv);
    size_t expected_min_len = offsetof(struct all_gpus_info_packet, gpus) + all_gpus->num_gpus_found * sizeof(struct gpu_info_packet);
    if ((unsigned int)len < (NLMSG_HDRLEN + expected_min_len) && all_gpus->num_gpus_found > 0) {
         update_status_label("Error: Received truncated multi-GPU message data."); return;
    }

    if (all_gpus->num_gpus_found == 0) { update_status_label("No GPUs reported by kernel module."); return; }

    char bus_s[MAX_VALUE_LEN], slot_s[MAX_VALUE_LEN], func_s[MAX_VALUE_LEN],
         vendor_id_s[MAX_VALUE_LEN], device_id_s[MAX_VALUE_LEN], updated_s[MAX_VALUE_LEN];
    char *vendor_name_p = NULL; char *device_name_p = NULL; char *util_p = NULL;

    time_t now = time(NULL); struct tm *t = localtime(&now);
    strftime(updated_s, sizeof(updated_s)-1, "%H:%M:%S", t);

    for (int i = 0; i < all_gpus->num_gpus_found && i < MAX_GPUS_SUPPORTED; ++i) {
        if (!all_gpus->gpus[i].is_valid) continue;
        GtkTreeIter iter; gtk_list_store_append(list_store, &iter);
        struct gpu_info_packet *gpu = &all_gpus->gpus[i];

        snprintf(bus_s, sizeof(bus_s), "%02u", gpu->bus);
        snprintf(slot_s, sizeof(slot_s), "%02u", gpu->slot);
        snprintf(func_s, sizeof(func_s), "%02u", gpu->function);
        snprintf(vendor_id_s, sizeof(vendor_id_s), "0x%04X", gpu->vendor_id);
        snprintf(device_id_s, sizeof(device_id_s), "0x%04X", gpu->device_id);

        vendor_name_p = get_vendor_name_from_pci_ids(gpu->vendor_id);
        device_name_p = get_device_name_from_pci_ids(gpu->vendor_id, gpu->device_id);
        util_p = get_gpu_utilization_from_sysfs(gpu->bus, gpu->slot, gpu->function); // Get utilization

        gtk_list_store_set(list_store, &iter,
                           COLUMN_BUS, bus_s, COLUMN_SLOT, slot_s, COLUMN_FUNCTION, func_s,
                           COLUMN_VENDOR_ID, vendor_id_s, COLUMN_VENDOR_NAME, vendor_name_p ? vendor_name_p : "Err",
                           COLUMN_DEVICE_ID, device_id_s, COLUMN_DEVICE_NAME, device_name_p ? device_name_p : "Err",
                           COLUMN_UTILIZATION, util_p ? util_p : "N/A", // Display utilization
                           COLUMN_LAST_UPDATED, updated_s, -1);
        if (vendor_name_p) free(vendor_name_p); if (device_name_p) free(device_name_p); if (util_p) free(util_p);
        vendor_name_p = NULL; device_name_p = NULL; util_p = NULL;
    }
    snprintf(status_msg_buf, sizeof(status_msg_buf), "Found %d GPU(s). Last updated: %s", all_gpus->num_gpus_found, updated_s);
    update_status_label(status_msg_buf);
}

// --- Timer and Button Callbacks ---
static gboolean periodic_scan_callback(gpointer data) { /* ... (same as before) ... */
    if (!is_scanning) { timer_id = 0; return G_SOURCE_REMOVE; }
    perform_scan_and_update_display();
    return G_SOURCE_CONTINUE;
}
void start_stop_scan_callback(GtkWidget *widget, gpointer data) { /* ... (same as before) ... */
    if (!is_scanning) {
        is_scanning = TRUE; gtk_button_set_label(scan_button, "Stop Real-time Scan");
        perform_scan_and_update_display();
        if (timer_id == 0) timer_id = g_timeout_add_seconds(UPDATE_INTERVAL_SECONDS, periodic_scan_callback, NULL);
    } else {
        is_scanning = FALSE; gtk_button_set_label(scan_button, "Start Real-time Scan");
        if (timer_id != 0) { g_source_remove(timer_id); timer_id = 0; }
        update_status_label("Real-time scanning stopped.");
    }
}

// --- Setup Functions ---
void setup_netlink() { /* ... (same as before) ... */
    struct sockaddr_nl src_addr;
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0) { perror("socket"); fprintf(stderr, "CRITICAL: Netlink socket creation failed.\n"); exit(EXIT_FAILURE); }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK; src_addr.nl_pid = getpid();
    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind"); fprintf(stderr, "CRITICAL: Netlink socket bind failed.\n"); close(sock_fd); sock_fd = -1; exit(EXIT_FAILURE);
    }
    printf("GUI_DEBUG: Netlink socket setup complete.\n"); fflush(stdout);
}

GtkWidget* create_tree_view(void) {
    GtkWidget *tree_view; GtkCellRenderer *renderer; GtkTreeViewColumn *column;
    list_store = gtk_list_store_new(NUM_DISPLAY_COLUMNS_ACTIVITY, // Updated enum count
                                    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, /* Util */
                                    G_TYPE_STRING  /* Last Updated */);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_view), TRUE);

    const char *column_titles[] = {"Bus", "Slot", "Func", "Vendor ID", "Vendor Name", "Device ID", "Device Name",
                                   "Util (%)", /* Temp removed */ "Updated"};
    // Adjusted column widths
    int column_min_widths[] = {40, 40, 40, 80, 200, 80, 220, 70, /* Temp removed */ 80};


    for (int i = 0; i < NUM_DISPLAY_COLUMNS_ACTIVITY; i++) {
        renderer = gtk_cell_renderer_text_new();
        // Optionally style the utilization column if it's "N/A"
        // if (i == COLUMN_UTILIZATION) {
        //      g_object_set(renderer, "foreground", "gray", "style", PANGO_STYLE_ITALIC, NULL);
        // }
        column = gtk_tree_view_column_new_with_attributes(column_titles[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_min_width(column, column_min_widths[i]);
        gtk_tree_view_column_set_sort_column_id(column, i);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    }
    return tree_view;
}

// --- Main Application Logic ---
int main(int argc, char *argv[]) { /* ... (same as before, ensure status_label is created and added) ... */
    GtkWidget *window; GtkWidget *vbox; GtkWidget *tree_view_widget; GtkWidget *scroll;
    gtk_init(&argc, &argv); setup_netlink();
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GPU Info & Utilization Viewer");
    gtk_window_set_default_size(GTK_WINDOW(window), 980, 400); // Adjusted size
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_container_add(GTK_CONTAINER(window), vbox);
    scan_button = GTK_BUTTON(gtk_button_new_with_label("Start Real-time Scan"));
    g_signal_connect(scan_button, "clicked", G_CALLBACK(start_stop_scan_callback), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scan_button), FALSE, FALSE, 0);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    tree_view_widget = create_tree_view(); gtk_container_add(GTK_CONTAINER(scroll), tree_view_widget);
    status_label = GTK_LABEL(gtk_label_new("GUI Initialized. Click 'Start Real-time Scan'."));
    gtk_label_set_xalign(status_label, 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(status_label), FALSE, FALSE, 5);
    gtk_widget_show_all(window);
    printf("GUI_DEBUG: Application initialized. GTK main loop starting.\n"); fflush(stdout);
    gtk_main();
    printf("GUI_DEBUG: GTK main loop ended. Cleaning up...\n"); fflush(stdout);
    if (timer_id != 0) { g_source_remove(timer_id); }
    if (sock_fd >= 0) { close(sock_fd); }
    if (list_store != NULL) { g_object_unref(list_store); }
    printf("GUI_DEBUG: Application exiting.\n"); fflush(stdout);
    return 0;
}
