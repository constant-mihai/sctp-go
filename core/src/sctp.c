#include <assert.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>

#include "log.h"
#include "sctp.h"

int
sctp_get_saddr(int af, const char* ip, uint16_t port,
               struct sockaddr *saddr /*out*/, int *saddr_len /*out*/)
{
    assert(ip != NULL && saddr != NULL && saddr_len != NULL);
    switch(af) {
        case AF_INET:
            struct sockaddr_in *saddr_in = (struct sockaddr_in*) saddr;
            memset(saddr_in, 0, sizeof(*saddr_in));
            saddr_in->sin_family = AF_INET;
            saddr_in->sin_port = htons(port);
            if(inet_pton(af, ip, &saddr_in->sin_addr.s_addr) <= 0) {
                return -1;
            }
            *saddr_len = sizeof(*saddr_in);
            break;
        case AF_INET6:
            LOG("IPv6 not implemented");
            return -1;
            break;
        default:
            return -1;
            break;
    }

    return 0;
}

int
sctp_get_ip_str(const struct sockaddr *saddr, char *buf /* out */, int buf_len, uint16_t *port /* out */)
{
    assert(saddr != NULL && buf != NULL && port != NULL);
    switch(saddr->sa_family) {
        case AF_INET:
            const struct sockaddr_in *saddr_in = (const struct sockaddr_in*) saddr;
            if (inet_ntop(AF_INET, &saddr_in->sin_addr, buf, buf_len) == NULL) {
                LOG("Cannot convert IP to text");
                return -1;
            }
            *port = ntohs(saddr_in->sin_port);
            break;
        case AF_INET6:
            LOG("IPv6 not implemented");
            return -1;
            break;
        default:
            LOG("Unkown family");
            return -1;
            break;
    }

    return 0;
}

int
sctp_listen(int sockfd)
{
    if (listen(sockfd, LISTEN_QUEUE_SIZE)) {
        LOG("listen() error: %d %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

// TODO: setsockopt SOCK_NONBLOCK failed: Protocol not available
void sctp_option_set_nonblocking(int sockfd, void *toggle) {
    if (setsockopt(sockfd, IPPROTO_SCTP, SOCK_NONBLOCK, toggle, sizeof(int)) < 0) {
        LOG("setsockopt SOCK_NONBLOCK failed: %s", strerror(errno));
    }
}

void sctp_option_set_interleave(int sockfd, void *toggle) {
    if (setsockopt(sockfd,
                   IPPROTO_SCTP,
                   SCTP_FRAGMENT_INTERLEAVE,
                   toggle,
                   sizeof(int))) {
        LOG("setsockopt() error: SCTP_FRAGMENT_INTERLEAVE: %s", strerror(errno));
    }
}

void sctp_option_set_partial_delivery_point(int sockfd, void *toggle) {
    // TODO: does setting this to 0 means disable?
    // TODO: is there any portability issues with other OSes?
    if(setsockopt(sockfd,
                  IPPROTO_SCTP,
                  SCTP_PARTIAL_DELIVERY_POINT,
                  toggle,
                  sizeof(int))) {
        LOG("setsockopt() error: SCTP_PARTIAL_DELIVERY_POINT: %s", strerror(errno));
    }
}

void sctp_option_subscribe_to_events(int sockfd, void *events) {
    if(setsockopt(sockfd, SOL_SCTP, SCTP_EVENTS, (void*)events, sizeof(struct sctp_event_subscribe))) {
        LOG("setsockopt() error: SCTP_EVENTS: %s", strerror(errno));
    }
}

int
sctp_socket(const char *ip, uint16_t port, sctp_options_container_t options_container)
{
    int sockfd;
    sockfd = socket(PF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);

    if (sockfd < 0) {
        LOG("error opening socket %s", strerror(errno));
        return -1;
    }

    for (int i = 0; i < options_container.len; i++) {
        options_container.options[i].fn(sockfd, options_container.options[i].args);
    }

    struct sockaddr saddr;
    int saddr_len = 0;

    if (sctp_get_saddr(AF_INET, ip, port, &saddr, &saddr_len)) {
        LOG("error setting ip address %s:%d", ip, port);
        return -1;
    }

    if(bind(sockfd, &saddr, saddr_len)) {
        LOG("bind() error: %d %s", errno, strerror(errno));
        return -1;
    }

    return sockfd;
}
