#include "log.h"
#include "mmsg.h"

#define __USE_GNU
#include <assert.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

mmsg_t *mmsg_create(uint8_t vec_len, uint16_t bufsize) {
    mmsg_t *mmsg = (mmsg_t*) calloc(1, sizeof(mmsg_t));
    mmsg->vec = (struct mmsghdr*) calloc(vec_len, sizeof(struct mmsghdr));
    for (size_t i = 0; i < vec_len; i++) {
        // alloc buf
        char *buf = (char*) calloc(bufsize, sizeof(char));

        // alloc iovec
        struct iovec *iov = (struct iovec*) calloc(1, sizeof(struct iovec));
        iov->iov_base = buf;
        iov->iov_len = bufsize;

        // assign to mmsghdr
        mmsg->vec[i].msg_hdr.msg_iov = iov;
        mmsg->vec[i].msg_hdr.msg_iovlen = 1;
    }
    mmsg->vec_len = vec_len;
    mmsg->bufsize = bufsize;

    return mmsg;
}

void mmsg_destroy(mmsg_t **mmsg) {
    if (*mmsg == NULL) return;

    mmsg_t *deref = *mmsg;
    for (size_t i = 0; i < deref->vec_len; i++) {
        struct msghdr msg_hdr = deref->vec[i].msg_hdr;
        free(msg_hdr.msg_iov->iov_base);
        msg_hdr.msg_iov->iov_base = NULL;
        free(msg_hdr.msg_iov);
        msg_hdr.msg_iov = NULL;
    }

    free(deref->vec);
    deref->vec = NULL;
    free(*mmsg);
    *mmsg = NULL;
}

int mmsg_dump_hex(const char *in, int start, int len, int stop, char *out) {
    int n = 0;
    for (int i = start; i < stop && i < start+len+1; i++) {
        n += sprintf(out+n, "%02x ", in[i]);
    }

    return n;
}

int mmsg_dump_str(const char *in, int start, int len, int stop, char *out) {
    int n = 0;
    for (int i = start; i < stop && i < start+len+1; i++) {
        unsigned char c;
        c = in[i];
        if (c < 32 || c > 127) {
            c = '.';
        }
        n += sprintf(out+n, "%c ", c);
    }

    return n;
}

int mmsg_buf_hex_dump(const char* in, size_t len, size_t offset, char *out) {
    int n = 0;
    char *buf = out + offset;
    for (size_t i = 0; i < len; i+=16) {
        n += sprintf(buf+n, "%04lx    ", i+1);
        n += mmsg_dump_hex(in+i, i, 16, len, buf+n);
        n += sprintf(buf+n, "   ");
        n += mmsg_dump_str(in+i, i, 16, len, buf+n);
        n += sprintf(buf+n, "\n");
    }
    
    return n;
}

// TODO: consider creating a dump_into_buf.
void mmsg_message_dump(mmsg_t *mmsg, int nmsg) {
    assert(mmsg != NULL && "container is null");
    char buf[6553];
    int n = 0;
    for (int i = 0; i < nmsg; i++) {
        struct msghdr hdr = mmsg->vec[i].msg_hdr;
        n += sprintf(buf+n, "iovec[%d]:\n", i);

        if (hdr.msg_flags & MSG_NOTIFICATION) {
            const union sctp_notification *notification = hdr.msg_iov->iov_base;
            n += sprintf(buf+n, "notification, type: %d\n", notification->sn_header.sn_type);
        }
        int hdr_len = mmsg->vec[i].msg_len;
        if (hdr_len == 0) continue;
        for (size_t vec_len = 0; vec_len < hdr.msg_iovlen; vec_len++) {
            n += mmsg_buf_hex_dump(hdr.msg_iov->iov_base,
                                   hdr_len,
                                   n,
                                   buf);
        }
    }

    if (n > 0) LOG("%s", buf);
}

// mmsg_send is a non-blocking operation and uses the mmsg_t container to send messages.
// Lifespan of the container is managed outside of this function.
int mmsg_send(mmsg_t *mmsg, int nmsg, int sockfd) {
    assert(mmsg != NULL && "container is null");
    // AFAIU, sendmmsg working in non-blocking mode doesn't
    // depend on socket options, but rather on MSG_DONTWAIT.
    // This means that EAGAIN and EWOULDBLOCK can be handled here.
    // via a retry loop.
    // However, such an encapsulated retry loop cannot be terminated by the caller.
    return sendmmsg(sockfd, mmsg->vec, nmsg, MSG_DONTWAIT | MSG_NOSIGNAL);
}

// mmsg_recv is a non-blocking operation and uses the mmsg_t container.
// Errors have to be handled outside.
// This includes EAGAIN and EWOULDBLOCK.
int mmsg_recv(mmsg_t *mmsg, int sockfd) {
    assert(mmsg != NULL && "container is null");
    // AFAIU, recvmmsg working in non-blocking mode doesn't
    // depend on socket options, but rather on MSG_DONTWAIT.
    return recvmmsg(sockfd, mmsg->vec, mmsg->vec_len, MSG_DONTWAIT, NULL);
}

mmsg_iterator_t mmsg_get_iterator(const mmsg_t *mmsg) {
    assert(mmsg != NULL && "container is null");
    mmsg_iterator_t iterator = {
        .mmsg = mmsg,
        .index = -1,
    };
    return iterator;
}

mmsg_bytes_t mmsg_iterator_next(mmsg_iterator_t* iterator) {
    assert(iterator != NULL && "iterator is null");
    mmsg_bytes_t bytes = {
        .buf = NULL,
        .len = 0,
    };
    if (iterator->index == iterator->mmsg->vec_len - 1) {
        iterator->index = -1;
        return bytes;
    }

    iterator->index++;
    bytes.buf = iterator->mmsg->vec[iterator->index].msg_hdr.msg_iov->iov_base;
    bytes.len = iterator->mmsg->vec[iterator->index].msg_len;
    return bytes;
}
