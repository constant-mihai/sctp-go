#pragma once

#define __USE_GNU
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netdb.h>
#include <syslog.h>
#include <errno.h>

#include <netinet/sctp.h>

#define LISTEN_QUEUE_SIZE 32
#define BUF_SIZE 500

typedef void (*sctp_option_f)(int, void*);

typedef struct {
    sctp_option_f fn;
    void *args;
} sctp_option_t;

typedef struct {
    sctp_option_t *options;
    int len;
} sctp_options_container_t;

int sctp_get_saddr(int af, const char* ip, uint16_t port,
                   struct sockaddr *saddr /*out*/, int *saddr_len /*out*/);
int sctp_get_ip_str(const struct sockaddr *saddr,
                    char *buf /* out */,
                    int buf_len,
                    uint16_t *port /* out */);
int sctp_listen(int sockfd);
int sctp_socket(const char *ip, uint16_t port, sctp_options_container_t options_container);

// options
void sctp_option_set_nonblocking(int sockfd, void *args);
void sctp_option_set_interleave(int sockfd, void *args);
void sctp_option_set_partial_delivery_point(int sockfd, void *args);
void sctp_option_subscribe_to_events(int sockfd, void *args);
