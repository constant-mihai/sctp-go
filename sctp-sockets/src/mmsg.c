#include "log.h"
#include "mmsg.h"

#define __USE_GNU
#include <errno.h>
#include <netinet/ip.h>
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

void mmsg_message_dump(mmsg_t *mmsg, int nmsg) {
    char buf[6553];
    int n = 0;
    for (int i = 0; i < nmsg; i++) {
        struct msghdr hdr = mmsg->vec[i].msg_hdr;
        int hdr_len = mmsg->vec[i].msg_len;
        if (hdr_len == 0) continue;
        n += sprintf(buf+n, "iovec[%d]:\n", i);
        for (size_t vec_len = 0; vec_len < hdr.msg_iovlen; vec_len++) {
            n += mmsg_buf_hex_dump(hdr.msg_iov->iov_base,
                                   hdr_len,
                                   n,
                                   buf);
        }
    }

    if (n > 0) LOG("%s", buf);
}

int mmsg_send(mmsg_t *mmsg, int nmsg, int sockfd) {
    int nsent = 0;
    do {
        int ret = sendmmsg(sockfd, mmsg->vec, nmsg, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (ret == -1 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
            LOG("error on sendmmsg(): %s", strerror(errno));
            return ret;
        }
        nsent += ret;
    } while (nsent < nmsg);
    return nsent;
}

int mmsg_recv(mmsg_t *mmsg, int sockfd) {
    int nread;

    nread = recvmmsg(sockfd, mmsg->vec, mmsg->vec_len, MSG_DONTWAIT, NULL);
    if (nread == -1 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
        // TODO: this can potentially flood the log.
        LOG("error on recvmmsg(): %s", strerror(errno));
        //exit(EXIT_FAILURE);
    }

    return nread;
}
