#pragma once

#include <stdint.h>

typedef struct {
    struct mmsghdr *vec;
    uint8_t vec_len;
    uint16_t bufsize;
} mmsg_t;

mmsg_t *mmsg_create(uint8_t vec_len, uint16_t bufsize);
void mmsg_destroy(mmsg_t **multi_message);

void mmsg_send(mmsg_t *mmsg, int nmsg, int sockfd);

int mmsg_recv(mmsg_t *mmsg, int sockfd);

void mmsg_message_dump(mmsg_t *mmsg, int nmsg);
