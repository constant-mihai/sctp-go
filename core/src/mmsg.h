#pragma once

#include <stdint.h>

typedef struct mmsg {
    struct mmsghdr *vec;
    uint8_t vec_len;
    uint16_t bufsize;
} mmsg_t;

typedef struct {
    const char *buf;
    int len;
} mmsg_bytes_t;

// not thread safe.
typedef struct mmsg_iterator {
    // internal use
    const mmsg_t* mmsg;
    int index;
} mmsg_iterator_t;


mmsg_t *mmsg_create(uint8_t vec_len, uint16_t bufsize);
void mmsg_destroy(mmsg_t **multi_message);
int mmsg_send(mmsg_t *mmsg, int nmsg, int sockfd);
int mmsg_recv(mmsg_t *mmsg, int sockfd);
void mmsg_message_dump(mmsg_t *mmsg, int nmsg);
mmsg_iterator_t mmsg_get_iterator(const mmsg_t *mmsg);
mmsg_bytes_t mmsg_iterator_next(mmsg_iterator_t* iterator);
