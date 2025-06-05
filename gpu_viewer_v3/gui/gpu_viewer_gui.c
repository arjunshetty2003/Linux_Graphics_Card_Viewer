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
#include <dirent.h> 
#include <limits.h> 

#include "../include/gpu_proto.h"

GtkListStore *list_store;
GtkButton *scan_button;
GtkLabel *status_label;
GtkWidget *main_window; // Reference to the main window for dialog parent
int sock_fd = -1;
gboolean is_scanning = FALSE;
guint timer_id = 0;

#define UPDATE_INTERVAL_SECONDS 3
#define MAX_VALUE_LEN 256
#define PCI_IDS_PATH "/usr/share/hwdata/pci.ids"

enum {
    COLUMN_BUS = 0, COLUMN_SLOT, COLUMN_FUNCTION, COLUMN_VENDOR_ID,
    COLUMN_VENDOR_NAME, COLUMN_DEVICE_ID, COLUMN_DEVICE_NAME,
    COLUMN_UTILIZATION, COLUMN_LAST_UPDATED,
    NUM_DISPLAY_COLUMNS_FINAL
};

// --- PCI ID Lookup Functions (ensure these return char* to be freed) ---
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
    FILE *fp; char line[512]; char target_vid_str[10]; char *name_found = NULL;
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
            char current_vid_str[5] = {0}; strncpy(current_vid_str, line, 4);
            if (strcmp(current_vid_str, target_vid_str) == 0) {
                char *name_ptr = line + 4; while (*name_ptr && isspace((unsigned char)*name_ptr)) name_ptr++;
                name_found = strdup(trim_whitespace(name_ptr)); break;
            }
        }
    }
    fclose(fp);
    if (!name_found) {
        name_found = (char*)malloc(MAX_VALUE_LEN);
        if (name_found) snprintf(name_found, MAX_VALUE_LEN, "Unknown (0x%04X)", vendor_id);
        else name_found = strdup("Unknown");
    }
    return name_found;
}

char* get_device_name_from_pci_ids(unsigned short vendor_id, unsigned short device_id) {
    FILE *fp; char line[512]; char target_vendor_id_str[10], target_device_id_str[10];
    char *name_found = NULL; int in_target_vendor_section = 0;
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
            char current_vid_str[5] = {0}; strncpy(current_vid_str, line, 4);
            if (strcmp(current_vid_str, target_vendor_id_str) == 0) in_target_vendor_section = 1;
        } else if (in_target_vendor_section && line[0] == '\t' && line[1] != '\t') {
            char current_did_str[5] = {0}; strncpy(current_did_str, line + 1, 4);
            if (strcmp(current_did_str, target_device_id_str) == 0) {
                char *name_ptr = line + 1 + 4; while (*name_ptr && isspace((unsigned char)*name_ptr)) name_ptr++;
                name_found = strdup(trim_whitespace(name_ptr)); break;
            }
        } else if (line[0] != '\t' && in_target_vendor_section) break;
    }
    fclose(fp);
    if (!name_found) {
        name_found = (char*)malloc(MAX_VALUE_LEN);
        if (name_found) snprintf(name_found, MAX_VALUE_LEN, "Unknown (0x%04X)", device_id);
        else name_found = strdup("Unknown");
    }
    return name_found;
}

// --- Sysfs/Tool GPU Utilization Reading Function (Vendor-Aware Best Effort) ---
char* get_gpu_utilization_info(unsigned char bus, unsigned char slot, unsigned char func, unsigned short vendor_id) {
    char pci_domain_bus_slot_func[32];
    char command[512];
    char output_line[256];
    FILE *fp_tool_output;
    char *util_display_str = NULL;

    snprintf(pci_domain_bus_slot_func, sizeof(pci_domain_bus_slot_func), "0000:%02x:%02x.%x", bus, slot, func);
    printf("GUI_DEBUG: ExternalTool: Trying for PCI %s (Vendor: 0x%04X)\n", pci_domain_bus_slot_func, vendor_id);

    if (vendor_id == 0x8086) { // Intel GPU
        // For Intel, we will indicate that double-clicking the row launches intel_gpu_top
        return strdup("Intel (Double-click for intel_gpu_top)");
    } else if (vendor_id == 0x1002) { // AMD GPU
        // Try sysfs path first
        DIR *drm_dir = opendir("/sys/class/drm/");
        char sysfs_busy_path[PATH_MAX];
        char drm_card_path_base[PATH_MAX];
        char drm_device_link_target[PATH_MAX];
        struct dirent *drm_entry;
        if (drm_dir) {
            while ((drm_entry = readdir(drm_dir)) != NULL) {
                if (strncmp(drm_entry->d_name, "card", 4) == 0 && isdigit(drm_entry->d_name[4])) {
                    snprintf(drm_card_path_base, sizeof(drm_card_path_base), "/sys/class/drm/%s/device", drm_entry->d_name);
                    ssize_t len = readlink(drm_card_path_base, drm_device_link_target, sizeof(drm_device_link_target) - 1);
                    if (len != -1) {
                        drm_device_link_target[len] = '\0';
                        char *pci_basename = strrchr(drm_device_link_target, '/');
                        if (pci_basename && strcmp(pci_basename + 1, pci_domain_bus_slot_func) == 0) {
                            snprintf(sysfs_busy_path, sizeof(sysfs_busy_path), "/sys/class/drm/%s/device/gpu_busy_percent", drm_entry->d_name);
                            FILE *fp_amd = fopen(sysfs_busy_path, "r");
                            if (fp_amd) {
                                char amd_util_str_buf[32];
                                if (fgets(amd_util_str_buf, sizeof(amd_util_str_buf), fp_amd)) {
                                    int util_val = atoi(trim_whitespace(amd_util_str_buf));
                                    util_display_str = (char*)malloc(MAX_VALUE_LEN);
                                    if(util_display_str) snprintf(util_display_str, MAX_VALUE_LEN, "%d %% (AMD SysFS)", util_val);
                                }
                                fclose(fp_amd);
                                if (util_display_str) break; 
                            }
                        }
                    }
                }
            }
            closedir(drm_dir);
            if (util_display_str) return util_display_str;
        }
        return strdup("AMD (Use radeontop)");
    } else if (vendor_id == 0x10DE) { // NVIDIA GPU
        snprintf(command, sizeof(command), "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null | head -n 1");
        printf("GUI_DEBUG: Executing for NVIDIA: %s\n", command);
        fp_tool_output = popen(command, "r");
        if (fp_tool_output) {
            if (fgets(output_line, sizeof(output_line), fp_tool_output)) {
                int util_val = atoi(trim_whitespace(output_line));
                util_display_str = (char*)malloc(MAX_VALUE_LEN);
                if(util_display_str) snprintf(util_display_str, MAX_VALUE_LEN, "%d %% (NVIDIA)", util_val);
            }
            pclose(fp_tool_output);
            if (util_display_str) return util_display_str;
            else return strdup("NVIDIA (Parse Err)");
        } else {
            printf("GUI_DEBUG: Failed to execute nvidia-smi. Is it installed and in PATH?\n");
            return strdup("NVIDIA (Tool N/A)");
        }
    }
    return strdup("N/A");
}

void clear_list_store(void) { if (!list_store) return; gtk_list_store_clear(list_store); }
void update_status_label(const char *message) { if (!status_label) return; gtk_label_set_text(status_label, message); }

void perform_scan_and_update_display(void) {
    char recv_buf[MAX_PAYLOAD];
    struct nlmsghdr *nlh_recv;
    struct sockaddr_nl src_addr; socklen_t addr_len = sizeof(src_addr); int len;
    char status_msg_buf[MAX_VALUE_LEN];

    struct nlmsghdr *nlh_send; struct sockaddr_nl dest_addr; struct iovec iov;
    struct msghdr msg_send_hdr; char send_buf[NLMSG_SPACE(MAX_PAYLOAD)];
    char user_msg[] = "GET_ALL_GPUS_INFO"; int ret;

    update_status_label("Scanning...");
    if (sock_fd < 0) {
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
    if (ret < 0) {
        snprintf(status_msg_buf, sizeof(status_msg_buf), "Error: sendmsg failed: %s", strerror(errno));
        clear_list_store(); update_status_label(status_msg_buf); perror("sendmsg"); return;
    }

    nlh_recv = (struct nlmsghdr *)recv_buf;
    len = recvfrom(sock_fd, nlh_recv, MAX_PAYLOAD, 0, (struct sockaddr *)&src_addr, &addr_len);
    clear_list_store();

    if (len < 0) {
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
        util_p = get_gpu_utilization_info(gpu->bus, gpu->slot, gpu->function, gpu->vendor_id);

        gtk_list_store_set(list_store, &iter,
                           COLUMN_BUS, bus_s, COLUMN_SLOT, slot_s, COLUMN_FUNCTION, func_s,
                           COLUMN_VENDOR_ID, vendor_id_s, COLUMN_VENDOR_NAME, vendor_name_p ? vendor_name_p : "Err",
                           COLUMN_DEVICE_ID, device_id_s, COLUMN_DEVICE_NAME, device_name_p ? device_name_p : "Err",
                           COLUMN_UTILIZATION, util_p ? util_p : "N/A",
                           COLUMN_LAST_UPDATED, updated_s, -1);
        if (vendor_name_p) free(vendor_name_p); if (device_name_p) free(device_name_p); if (util_p) free(util_p);
        vendor_name_p = NULL; device_name_p = NULL; util_p = NULL;
    }
    snprintf(status_msg_buf, sizeof(status_msg_buf), "Found %d GPU(s). Last updated: %s", all_gpus->num_gpus_found, updated_s);
    update_status_label(status_msg_buf);
}

// --- ROW ACTIVATED (DOUBLE-CLICK) CALLBACK ---
static void on_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column_activated, gpointer user_data) {
    GtkTreeModel *model; GtkTreeIter iter;
    char *vid_str_raw, *did_str_raw; // To get the raw IDs for vendor check
    char *bus_str, *slot_str, *func_str, *vname_str, *dname_str, *util_str, *updated_str;
    char dialog_text[1024];
    char command_to_run[512]; // For launching external terminal

    model = gtk_tree_view_get_model(tree_view);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        // Get all data first
        gtk_tree_model_get(model, &iter,
                           COLUMN_BUS, &bus_str, COLUMN_SLOT, &slot_str, COLUMN_FUNCTION, &func_str,
                           COLUMN_VENDOR_ID, &vid_str_raw, COLUMN_VENDOR_NAME, &vname_str,
                           COLUMN_DEVICE_ID, &did_str_raw, COLUMN_DEVICE_NAME, &dname_str,
                           COLUMN_UTILIZATION, &util_str, COLUMN_LAST_UPDATED, &updated_str, -1);

        unsigned short vendor_id = 0;
        if (vid_str_raw) sscanf(vid_str_raw, "0x%hx", &vendor_id); // Convert hex string to short

        if (vendor_id == 0x8086) { // Intel GPU
            // Try to find the card number for intel_gpu_top
            char card_num_str[4] = "";
            char pci_domain_bus_slot_func[32];
            snprintf(pci_domain_bus_slot_func, sizeof(pci_domain_bus_slot_func), "0000:%s:%s.%s", bus_str, slot_str, func_str);

            DIR *drm_dir = opendir("/sys/class/drm/");
            if (drm_dir) {
                struct dirent *drm_entry;
                char drm_device_link_path[PATH_MAX];
                char link_target[PATH_MAX];
                while ((drm_entry = readdir(drm_dir)) != NULL) {
                    if (strncmp(drm_entry->d_name, "card", 4) == 0 && isdigit(drm_entry->d_name[4])) {
                        snprintf(drm_device_link_path, sizeof(drm_device_link_path), "/sys/class/drm/%s/device", drm_entry->d_name);
                        ssize_t rlen = readlink(drm_device_link_path, link_target, sizeof(link_target) - 1);
                        if (rlen != -1) {
                            link_target[rlen] = '\0';
                            char *pci_basename = strrchr(link_target, '/');
                            if (pci_basename && strcmp(pci_basename + 1, pci_domain_bus_slot_func) == 0) {
                                strncpy(card_num_str, drm_entry->d_name + 4, sizeof(card_num_str) -1);
                                card_num_str[sizeof(card_num_str)-1] = '\0';
                                break;
                            }
                        }
                    }
                }
                closedir(drm_dir);
            }
            if (strlen(card_num_str) > 0) {
                 snprintf(command_to_run, sizeof(command_to_run), "gnome-terminal --title=\"Intel GPU Top (%s)\" -- sudo intel_gpu_top -d card%s", dname_str ? dname_str : vid_str_raw, card_num_str);
            } else {
                 snprintf(command_to_run, sizeof(command_to_run), "gnome-terminal --title=\"Intel GPU Top (%s)\" -- sudo intel_gpu_top", dname_str ? dname_str : vid_str_raw);
            }
            
            printf("GUI_DEBUG: Launching Intel GPU Top: %s\n", command_to_run);
            int ret = system(command_to_run); // system() is blocking, consider g_spawn_async for non-blocking
            if (ret == -1 || WIFEXITED(ret) && WEXITSTATUS(ret) != 0 ) {
                 GtkWidget *err_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                 "Failed to launch 'intel_gpu_top'.\nEnsure 'gnome-terminal' and 'intel-gpu-tools' are installed, and sudo permissions might be needed by intel_gpu_top.");
                gtk_window_set_title(GTK_WINDOW(err_dialog), "Error Launching Tool");
                gtk_dialog_run(GTK_DIALOG(err_dialog));
                gtk_widget_destroy(err_dialog);
            }
        } else { // For non-Intel GPUs, show the info dialog
            snprintf(dialog_text, sizeof(dialog_text),
                     "GPU Details (Snapshot):\n\n"
                     "PCI Address: %s:%s.%s\n"
                     "Vendor: %s (%s)\n"
                     "Device: %s (%s)\n"
                     "Current Info/Util: %s\n"
                     "Last Row Update: %s\n\n"
                     "Note: For comprehensive real-time performance, "
                     "specialized tools might be available for this vendor (e.g., radeontop, nvidia-smi).",
                     bus_str, slot_str, func_str, vid_str_raw, vname_str ? vname_str : "N/A",
                     did_str_raw, dname_str ? dname_str : "N/A", util_str ? util_str : "N/A", updated_str);

            GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                     GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", dialog_text);
            gtk_window_set_title(GTK_WINDOW(dialog), "GPU Details");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }

        // Free the strings obtained from gtk_tree_model_get
        g_free(bus_str); g_free(slot_str); g_free(func_str);
        g_free(vid_str_raw); g_free(vname_str);
        g_free(did_str_raw); g_free(dname_str);
        g_free(util_str); g_free(updated_str);
    }
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
    list_store = gtk_list_store_new(NUM_DISPLAY_COLUMNS_FINAL,
                                    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_view), TRUE);
    // Connect the row-activated signal for double-click
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);

    const char *column_titles[] = {"Bus", "Slot", "Func", "Vendor ID", "Vendor Name", "Device ID", "Device Name",
                                   "Util (%) / Info", "Updated"};
    int column_min_widths[] = {40, 40, 40, 80, 200, 80, 220, 200, 80}; // Adjusted util width

    for (int i = 0; i < NUM_DISPLAY_COLUMNS_FINAL; i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(column_titles[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_min_width(column, column_min_widths[i]);
        gtk_tree_view_column_set_sort_column_id(column, i);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    }
    return tree_view;
}

// --- Main Application Logic ---
int main(int argc, char *argv[]) {
    GtkWidget *vbox; GtkWidget *tree_view_widget; GtkWidget *scroll;
    gtk_init(&argc, &argv); setup_netlink();
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL); // Assign to global main_window
    gtk_window_set_title(GTK_WINDOW(main_window), "GPU Info Viewer (External Tool Integration)");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1050, 400);
    gtk_container_set_border_width(GTK_CONTAINER(main_window), 10);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6); gtk_container_add(GTK_CONTAINER(main_window), vbox);
    scan_button = GTK_BUTTON(gtk_button_new_with_label("Start Real-time Scan"));
    g_signal_connect(scan_button, "clicked", G_CALLBACK(start_stop_scan_callback), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scan_button), FALSE, FALSE, 0);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    tree_view_widget = create_tree_view(); gtk_container_add(GTK_CONTAINER(scroll), tree_view_widget);
    status_label = GTK_LABEL(gtk_label_new("GUI Initialized. Click 'Start Scan'. Double-click Intel GPU row for intel_gpu_top."));
    gtk_label_set_xalign(status_label, 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(status_label), FALSE, FALSE, 5);
    gtk_widget_show_all(main_window);
    printf("GUI_DEBUG: Application initialized. GTK main loop starting.\n"); fflush(stdout);
    gtk_main();
    printf("GUI_DEBUG: GTK main loop ended. Cleaning up...\n"); fflush(stdout);
    if (timer_id != 0) { g_source_remove(timer_id); }
    if (sock_fd >= 0) { close(sock_fd); }
    if (list_store != NULL) { g_object_unref(list_store); }
    printf("GUI_DEBUG: Application exiting.\n"); fflush(stdout);
    return 0;
}
