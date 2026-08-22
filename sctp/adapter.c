// std
#include <assert.h>
#include <string.h>

// src
#include "adapter.h"

int DefaultSctpServer(const char *ip, uint16_t port) {
    struct sctp_event_subscribe events;
    memset(&events, 0, sizeof(struct sctp_event_subscribe));
    events.sctp_data_io_event = 1;
    events.sctp_association_event = 1;
    events.sctp_address_event = 1;
    events.sctp_send_failure_event = 1;
    events.sctp_peer_error_event = 1;
    events.sctp_shutdown_event = 1;

    int toggle_off = 0;
    sctp_option_t socket_options[] = {
        { .fn = sctp_option_set_interleave, .args = &toggle_off },
        { .fn = sctp_option_set_partial_delivery_point, .args = &toggle_off },
        { .fn = sctp_option_subscribe_to_events, .args = &events},
    };
    sctp_options_container_t socket_options_container = {
        .options = socket_options,
        .len = sizeof(socket_options)/sizeof(socket_options[0]),
    };
    int sockfd = sctp_socket(ip, port,
                         socket_options_container);
    if (sockfd < 0) {
        return -1;
    }

    if (sctp_listen(sockfd)) {
        return -1;
    }

    return sockfd;
}

int DefaultSctpClient(const char *ip, uint16_t port) {
    struct sctp_event_subscribe events;
    memset(&events, 0, sizeof(struct sctp_event_subscribe));
    events.sctp_data_io_event = 1;
    events.sctp_association_event = 1;
    events.sctp_address_event = 1;
    events.sctp_send_failure_event = 1;
    events.sctp_peer_error_event = 1;
    events.sctp_shutdown_event = 1;

    int toggle_off = 0;
    sctp_option_t socket_options[] = {
        { .fn = sctp_option_set_interleave, .args = &toggle_off },
        { .fn = sctp_option_set_partial_delivery_point, .args = &toggle_off },
        { .fn = sctp_option_subscribe_to_events, .args = &events},
    };
    sctp_options_container_t socket_options_container = {
        .options = socket_options,
        .len = sizeof(socket_options)/sizeof(socket_options[0]),
    };
    int sockfd = sctp_socket(ip, port,
                             socket_options_container);
    if (sockfd < 0) {
        return -1;
    }

    return sockfd;
}

// TODO: for now it creates and destroyes mmsg on every call and then copies the buf over.
// this is not optimal. Possible solutions:
// 1. Use sendto insteand of mmsg_send.
// 2. a) Offer wrappers on top of mmsg, so that the go callers can use it to send.
//    b) Add a memory pool so that the mmsg doesn't get alloc'ed every time.
int SendMsg(int sockfd, const char* dst, int port, const char* buf, int len) {
    struct sockaddr saddr;
    int saddr_len;
    if (sctp_get_saddr(AF_INET, dst, port,
                       &saddr, &saddr_len)) {
        return -1;
    }

    // TODO: save this somewhere
    /* msg_hdr->msg_name = (void*)&saddr;*/
    /* msg_hdr->msg_namelen = saddr_len;*/

    return sendto(sockfd, buf, len, MSG_DONTWAIT, &saddr, saddr_len);
}

// TODO: should this be blocking instead?
// recvmsg rather than recvfrom: it reports msg_flags, which is the only way to
// tell an SCTP notification apart from user data. On entry *saddr_len must hold
// the size of saddr; on return it holds the size the kernel filled in.
int RecvMsg(int sockfd, char* buf, int buf_len, struct sockaddr *saddr,
            unsigned int *saddr_len, int *flags) {
    assert(saddr != NULL && saddr_len != NULL && buf != NULL && flags != NULL);
    struct iovec iov = {
        .iov_base = buf,
        .iov_len = buf_len,
    };
    struct msghdr msg = {
        .msg_name = saddr,
        .msg_namelen = *saddr_len,
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    int nread = recvmsg(sockfd, &msg, MSG_DONTWAIT);
    if (nread < 0) {
        return nread;
    }

    *saddr_len = msg.msg_namelen;
    *flags = msg.msg_flags;
    return nread;
}

int NotificationString(const char *buf, int buf_len, char *out, int out_len) {
    return sctp_notification_str(buf, buf_len, out, out_len);
}

int GetAddress(const struct sockaddr *saddr, char *buf, int buf_len, uint16_t *port) {
    assert(saddr != NULL && buf != NULL);
    return sctp_get_ip_str(saddr, buf, buf_len, port);
}

int SendMultiMsg(int sockfd, const char* dst, int port, mmsg_t *mmsg, int numMsg) {
    return mmsg_send(mmsg, numMsg, sockfd);
}

int RecvMultiMsg(int sockfd, mmsg_t *mmsg) {
    return mmsg_recv(mmsg, sockfd);
}

mmsg_t *CreateMmsg(int len, int bufsize) {
    return mmsg_create(len, bufsize);
}

void DestroyMmsg(mmsg_t **mmsg) {
    return mmsg_destroy(mmsg);
}
