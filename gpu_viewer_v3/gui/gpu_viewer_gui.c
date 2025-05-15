#include <gtk/gtk.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#include "../include/gpu_proto.h"

GtkTextBuffer *buffer;
int sock_fd;

void append_text(const char *msg) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, msg, -1);
}

void receive_gpu_info() {
    char buf[MAX_PAYLOAD];
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
    struct sockaddr_nl src_addr;
    socklen_t addr_len = sizeof(src_addr);

    int len = recvfrom(sock_fd, nlh, MAX_PAYLOAD, 0, (struct sockaddr *)&src_addr, &addr_len);
    if (len <= 0) return;

    struct gpu_info_packet *info = (struct gpu_info_packet *)NLMSG_DATA(nlh);
    char line[128];
    snprintf(line, sizeof(line), "GPU: Vendor=0x%04x Device=0x%04x Bus=%02x Slot=%02x Func=%02x\n",
             info->vendor_id, info->device_id, info->bus, info->slot, info->function);
    append_text(line);
}

void request_gpu_info(GtkWidget *widget, gpointer data) {
    struct nlmsghdr *nlh;
    struct sockaddr_nl dest_addr;
    struct iovec iov;
    struct msghdr msg;

    char dummy_msg[1] = {0};
    char buf[NLMSG_SPACE(MAX_PAYLOAD)];

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;

    nlh = (struct nlmsghdr *)buf;
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    memcpy(NLMSG_DATA(nlh), dummy_msg, 1);

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    sendmsg(sock_fd, &msg, 0);
    receive_gpu_info();  // immediate response
}

void setup_netlink() {
    struct sockaddr_nl src_addr;

    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0) {
        perror("socket");
        exit(1);
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));
}

int main(int argc, char *argv[]) {
    GtkWidget *window, *vbox, *button, *text_view, *scroll;

    gtk_init(&argc, &argv);
    setup_netlink();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Advanced GPU Viewer");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    button = gtk_button_new_with_label("Scan GPU Devices");
    g_signal_connect(button, "clicked", G_CALLBACK(request_gpu_info), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_container_add(GTK_CONTAINER(scroll), text_view);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
