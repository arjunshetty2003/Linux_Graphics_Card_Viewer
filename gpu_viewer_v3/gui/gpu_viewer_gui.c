#include <gtk/gtk.h>
#include <sys/socket.h>
#include <linux/netlink.h> // For struct nlmsghdr, NLMSG_DATA, NLMSG_SPACE, NETLINK_USER
#include <unistd.h>        // For getpid()
#include <string.h>        // For memset(), memcpy(), strerror()
#include <stdio.h>         // For printf(), perror(), snprintf()
#include <stdlib.h>        // For exit()
#include <errno.h>         // For errno
#include <time.h>          // For time(), localtime(), strftime()

// This header file defines NETLINK_USER, MAX_PAYLOAD, and struct gpu_info_packet
// It should be located in ../include/ relative to this C file.
#include "../include/gpu_proto.h"

// Global variables
GtkListStore *list_store;     // Data store for the GtkTreeView
GtkButton *scan_button;       // Button to start/stop scanning
int sock_fd = -1;             // File descriptor for the netlink socket
gboolean is_scanning = FALSE; // Flag to indicate if scanning is active
guint timer_id = 0;           // ID for the timer event

#define UPDATE_INTERVAL_SECONDS 5 // Update every 5 seconds

// Enum for GtkTreeView columns
enum {
    COLUMN_PROPERTY = 0,
    COLUMN_VALUE,
    NUM_COLUMNS
};

// Helper function to get vendor name (simplified)
const char* get_vendor_name(unsigned short vendor_id) {
    switch (vendor_id) {
        case 0x10DE: return "NVIDIA Corporation";
        case 0x8086: return "Intel Corporation";
        case 0x1002: return "Advanced Micro Devices, Inc. [AMD/ATI]";
        case 0x102B: return "Matrox Graphics, Inc.";
        case 0x10EC: return "Realtek Semiconductor Co., Ltd."; // Often network, but for example
        // Add more common vendors as needed
        default:     return "Unknown Vendor";
    }
}

// Helper function to get device name (simplified)
const char* get_device_name(unsigned short vendor_id, unsigned short device_id) {
    // NVIDIA Devices
    if (vendor_id == 0x10DE) {
        switch (device_id) {
            case 0x1EB8: return "GeForce GTX 1650 Ti Mobile";
            case 0x2488: return "GeForce RTX 3050 Laptop GPU";
            case 0x1F02: return "TU116 [GeForce GTX 1660 Ti]";
            case 0x2204: return "GA102 [GeForce RTX 3080]";
            // Add more NVIDIA devices
            default:     return "Unknown NVIDIA Device";
        }
    }
    // Intel Devices
    else if (vendor_id == 0x8086) {
        switch (device_id) {
            case 0x9A49: return "Iris Xe Graphics";
            case 0x3EA5: return "UHD Graphics 630 (Desktop)";
            case 0x5917: return "HD Graphics 630 (Kaby Lake GT2)";
            // Add more Intel devices
            default:     return "Unknown Intel Device";
        }
    }
    // AMD/ATI Devices
    else if (vendor_id == 0x1002) {
        switch (device_id) {
            case 0x73BF: return "Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]";
            case 0x67DF: return "Ellesmere [Radeon RX 470/480/570/570X/580/580X/590]";
            // Add more AMD devices
            default:     return "Unknown AMD/ATI Device";
        }
    }
    return "N/A (Unknown Vendor)";
}


// Helper function to add a row to the GtkListStore
void add_row_to_store(const char *property, const char *value) {
    GtkTreeIter iter;
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter,
                       COLUMN_PROPERTY, property,
                       COLUMN_VALUE, value,
                       -1); // -1 to terminate the list of column/value pairs
}

// Clears all rows from the GtkListStore
void clear_list_store(void) {
    gtk_list_store_clear(list_store);
}

// Function to perform the scan, send request, receive response, and update the GtkTreeView
void perform_scan_and_update_display(void) {
    char recv_buf[MAX_PAYLOAD];
    struct nlmsghdr *nlh_recv;
    struct sockaddr_nl src_addr;
    socklen_t addr_len = sizeof(src_addr);
    int len;
    char value_str[256]; // Buffer for formatted output string

    // For sending the request
    struct nlmsghdr *nlh_send;
    struct sockaddr_nl dest_addr;
    struct iovec iov;
    struct msghdr msg_send;
    char send_buf[NLMSG_SPACE(MAX_PAYLOAD)];
    char user_msg[] = "GET_GPU_INFO";
    int ret;

    printf("GUI_DEBUG: Performing scan...\n");
    fflush(stdout);

    if (sock_fd < 0) {
        clear_list_store();
        add_row_to_store("Error", "Netlink socket not initialized.");
        if (is_scanning) {
            if (timer_id != 0) g_source_remove(timer_id);
            timer_id = 0;
            is_scanning = FALSE;
            gtk_button_set_label(scan_button, "Start Real-time Scan");
        }
        return;
    }

    // --- Send Request ---
    memset(send_buf, 0, sizeof(send_buf));
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;

    nlh_send = (struct nlmsghdr *)send_buf;
    nlh_send->nlmsg_len = NLMSG_SPACE(strlen(user_msg) + 1);
    nlh_send->nlmsg_pid = getpid();
    nlh_send->nlmsg_flags = 0;
    memcpy(NLMSG_DATA(nlh_send), user_msg, strlen(user_msg) + 1);

    iov.iov_base = (void *)nlh_send;
    iov.iov_len = nlh_send->nlmsg_len;

    memset(&msg_send, 0, sizeof(msg_send));
    msg_send.msg_name = (void *)&dest_addr;
    msg_send.msg_namelen = sizeof(dest_addr);
    msg_send.msg_iov = &iov;
    msg_send.msg_iovlen = 1;

    printf("GUI_DEBUG: Sending message to kernel: '%s'\n", user_msg);
    fflush(stdout);
    ret = sendmsg(sock_fd, &msg_send, 0);
    if (ret < 0) {
        snprintf(value_str, sizeof(value_str), "sendmsg failed: %s", strerror(errno));
        clear_list_store();
        add_row_to_store("Error", value_str);
        perror("GUI_ERROR_DETAIL (sendmsg)");
        return;
    }
    printf("GUI_DEBUG: sendmsg sent %d bytes.\n", ret);
    fflush(stdout);

    // --- Receive Response ---
    nlh_recv = (struct nlmsghdr *)recv_buf;
    printf("GUI_DEBUG: Attempting to receive message from kernel (recvfrom)...\n");
    fflush(stdout);

    len = recvfrom(sock_fd, nlh_recv, MAX_PAYLOAD, 0, (struct sockaddr *)&src_addr, &addr_len);

    printf("GUI_DEBUG: recvfrom returned %d.\n", len);
    fflush(stdout);

    clear_list_store(); // Clear previous info

    if (len < 0) {
        snprintf(value_str, sizeof(value_str), "recvfrom failed: %s", strerror(errno));
        add_row_to_store("Error", value_str);
        perror("GUI_ERROR_DETAIL (recvfrom)");
        return;
    }
    if (len == 0) {
        add_row_to_store("Info", "recvfrom returned 0 (kernel module might have closed or sent no data).");
        return;
    }
    // Check if the received message is large enough to contain our struct
    if (nlh_recv->nlmsg_len < NLMSG_HDRLEN || (unsigned int)len < nlh_recv->nlmsg_len || (unsigned int)len < (NLMSG_HDRLEN + sizeof(struct gpu_info_packet))) {
        add_row_to_store("Error", "Received corrupted or incomplete netlink message.");
        printf("GUI_DEBUG: Corrupt message: len=%d, nlmsg_len=%u, expected_payload_size=%zu\n", len, nlh_recv->nlmsg_len, sizeof(struct gpu_info_packet));
        return;
    }

    struct gpu_info_packet *info = (struct gpu_info_packet *)NLMSG_DATA(nlh_recv);

    snprintf(value_str, sizeof(value_str), "0x%04X", info->vendor_id);
    add_row_to_store("Vendor ID", value_str);
    add_row_to_store("Vendor Name", get_vendor_name(info->vendor_id)); // New

    snprintf(value_str, sizeof(value_str), "0x%04X", info->device_id);
    add_row_to_store("Device ID", value_str);
    add_row_to_store("Device Name", get_device_name(info->vendor_id, info->device_id)); // New


    snprintf(value_str, sizeof(value_str), "%u (0x%02X)", info->bus, info->bus);
    add_row_to_store("Bus", value_str);

    snprintf(value_str, sizeof(value_str), "%u (0x%02X)", info->slot, info->slot);
    add_row_to_store("Slot", value_str);

    snprintf(value_str, sizeof(value_str), "%u (0x%02X)", info->function, info->function);
    add_row_to_store("Function", value_str);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(value_str, sizeof(value_str)-1, "%Y-%m-%d %H:%M:%S", t);
    add_row_to_store("Last Updated", value_str);

    printf("GUI_DEBUG: Displayed received GPU info in table.\n");
    fflush(stdout);
}

// Timer callback function for periodic scanning
static gboolean periodic_scan_callback(gpointer data) {
    if (!is_scanning) {
        printf("GUI_DEBUG: Timer callback invoked, but scanning is stopped. Removing timer.\n");
        fflush(stdout);
        timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    printf("GUI_DEBUG: Timer triggered periodic scan.\n");
    fflush(stdout);
    perform_scan_and_update_display();
    return G_SOURCE_CONTINUE;
}

// Callback function for the "Start/Stop Real-time Scan" button click
void start_stop_scan_callback(GtkWidget *widget, gpointer data) {
    if (!is_scanning) {
        printf("GUI_DEBUG: 'Start Real-time Scan' button clicked.\n");
        fflush(stdout);
        is_scanning = TRUE;
        gtk_button_set_label(scan_button, "Stop Real-time Scan");
        perform_scan_and_update_display();
        if (timer_id == 0) {
             timer_id = g_timeout_add_seconds(UPDATE_INTERVAL_SECONDS, periodic_scan_callback, NULL);
             printf("GUI_DEBUG: Timer started with ID %u, interval %d seconds.\n", timer_id, UPDATE_INTERVAL_SECONDS);
             fflush(stdout);
        }
    } else {
        printf("GUI_DEBUG: 'Stop Real-time Scan' button clicked.\n");
        fflush(stdout);
        is_scanning = FALSE;
        gtk_button_set_label(scan_button, "Start Real-time Scan");
        if (timer_id != 0) {
            g_source_remove(timer_id);
            timer_id = 0;
            printf("GUI_DEBUG: Timer stopped.\n");
            fflush(stdout);
        }
        clear_list_store();
        add_row_to_store("Status", "Real-time scanning stopped. Click 'Start' to resume.");
    }
}

// Sets up the netlink socket
void setup_netlink() {
    struct sockaddr_nl src_addr;
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0) {
        perror("GUI_CRITICAL_ERROR (socket creation failed)");
        fprintf(stderr, "CRITICAL: Failed to create netlink socket. Ensure kernel module is loaded.\n");
        exit(EXIT_FAILURE);
    }
    printf("GUI_DEBUG: Netlink socket created successfully (fd: %d).\n", sock_fd);
    fflush(stdout);

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("GUI_CRITICAL_ERROR (socket bind failed)");
        fprintf(stderr, "CRITICAL: Failed to bind netlink socket.\n");
        close(sock_fd);
        sock_fd = -1;
        exit(EXIT_FAILURE);
    }
    printf("GUI_DEBUG: Netlink socket bound successfully.\n");
    fflush(stdout);
}

// Creates and sets up the GtkTreeView
GtkWidget* create_tree_view(void) {
    GtkWidget *tree_view;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    list_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Property", renderer, "text", COLUMN_PROPERTY, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 180); // Adjusted width
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Value", renderer, "text", COLUMN_VALUE, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 300); // Adjusted width
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    return tree_view;
}

// Main function
int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *tree_view_widget;
    GtkWidget *scroll;

    gtk_init(&argc, &argv);
    setup_netlink();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Advanced GPU Viewer (Tabular & Real-time)");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 500); // Adjusted size for wider table
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    scan_button = GTK_BUTTON(gtk_button_new_with_label("Start Real-time Scan"));
    g_signal_connect(scan_button, "clicked", G_CALLBACK(start_stop_scan_callback), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scan_button), FALSE, FALSE, 5);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    tree_view_widget = create_tree_view();
    gtk_container_add(GTK_CONTAINER(scroll), tree_view_widget);

    gtk_widget_show_all(window);

    clear_list_store();
    add_row_to_store("Status", "GUI Initialized. Click 'Start Real-time Scan'.");

    printf("GUI_DEBUG: Application initialized. Waiting for user interaction.\n");
    fflush(stdout);

    gtk_main();

    // Cleanup
    if (timer_id != 0) {
        g_source_remove(timer_id);
        timer_id = 0;
    }
    if (sock_fd >= 0) {
        printf("GUI_DEBUG: Closing netlink socket (fd: %d).\n", sock_fd);
        fflush(stdout);
        close(sock_fd);
    }
    if (list_store != NULL) { // Check if list_store was initialized
        g_object_unref(list_store); // Unreference the list store
    }


    printf("GUI_DEBUG: Application exiting.\n");
    fflush(stdout);
    return 0;
}
