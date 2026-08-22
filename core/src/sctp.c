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

static const char *
sctp_assoc_state_str(uint16_t state)
{
    switch (state) {
        case SCTP_COMM_UP:        return "COMM_UP";
        case SCTP_COMM_LOST:      return "COMM_LOST";
        case SCTP_RESTART:        return "RESTART";
        case SCTP_SHUTDOWN_COMP:  return "SHUTDOWN_COMP";
        case SCTP_CANT_STR_ASSOC: return "CANT_STR_ASSOC";
        default:                  return "UNKNOWN";
    }
}

static const char *
sctp_paddr_state_str(int state)
{
    switch (state) {
        case SCTP_ADDR_AVAILABLE:            return "ADDR_AVAILABLE";
        case SCTP_ADDR_UNREACHABLE:          return "ADDR_UNREACHABLE";
        case SCTP_ADDR_REMOVED:              return "ADDR_REMOVED";
        case SCTP_ADDR_ADDED:                return "ADDR_ADDED";
        case SCTP_ADDR_MADE_PRIM:            return "ADDR_MADE_PRIM";
        case SCTP_ADDR_CONFIRMED:            return "ADDR_CONFIRMED";
        case SCTP_ADDR_POTENTIALLY_FAILED:   return "ADDR_POTENTIALLY_FAILED";
        default:                             return "UNKNOWN";
    }
}

int
sctp_notification_str(const void *buf, int buf_len, char *out, int out_len)
{
    assert(buf != NULL && out != NULL);
    // sn_header: two u16 followed by a u32.
    const int header_len = 2 * sizeof(uint16_t) + sizeof(uint32_t);
    if (buf_len < header_len || out_len <= 0) {
        return -1;
    }

    const union sctp_notification *sn = (const union sctp_notification*) buf;
    int n = 0;
    switch (sn->sn_header.sn_type) {
        case SCTP_ASSOC_CHANGE: {
            const struct sctp_assoc_change *sac = &sn->sn_assoc_change;
            n = snprintf(out, out_len,
                         "ASSOC_CHANGE state=%s error=%u inbound=%u outbound=%u assoc_id=%d",
                         sctp_assoc_state_str(sac->sac_state),
                         sac->sac_error,
                         sac->sac_inbound_streams,
                         sac->sac_outbound_streams,
                         sac->sac_assoc_id);
            break;
        }
        case SCTP_PEER_ADDR_CHANGE: {
            const struct sctp_paddr_change *spc = &sn->sn_paddr_change;
            char ip[INET6_ADDRSTRLEN] = {0};
            uint16_t port = 0;
            if (sctp_get_ip_str((const struct sockaddr*) &spc->spc_aaddr,
                                ip, sizeof(ip), &port)) {
                snprintf(ip, sizeof(ip), "<unknown>");
            }
            n = snprintf(out, out_len,
                         "PEER_ADDR_CHANGE addr=%s:%u state=%s error=%d assoc_id=%d",
                         ip, port,
                         sctp_paddr_state_str(spc->spc_state),
                         spc->spc_error,
                         spc->spc_assoc_id);
            break;
        }
        case SCTP_REMOTE_ERROR: {
            const struct sctp_remote_error *sre = &sn->sn_remote_error;
            n = snprintf(out, out_len,
                         "REMOTE_ERROR error=%u len=%u assoc_id=%d",
                         ntohs(sre->sre_error),
                         sre->sre_length,
                         sre->sre_assoc_id);
            break;
        }
        case SCTP_SEND_FAILED: {
            const struct sctp_send_failed *ssf = &sn->sn_send_failed;
            n = snprintf(out, out_len,
                         "SEND_FAILED error=%u len=%u stream=%u assoc_id=%d",
                         ssf->ssf_error,
                         ssf->ssf_length,
                         ssf->ssf_info.sinfo_stream,
                         ssf->ssf_assoc_id);
            break;
        }
        case SCTP_SHUTDOWN_EVENT: {
            n = snprintf(out, out_len, "SHUTDOWN_EVENT assoc_id=%d",
                         sn->sn_shutdown_event.sse_assoc_id);
            break;
        }
        case SCTP_ADAPTATION_INDICATION: {
            n = snprintf(out, out_len, "ADAPTATION_INDICATION ind=%u assoc_id=%d",
                         sn->sn_adaptation_event.sai_adaptation_ind,
                         sn->sn_adaptation_event.sai_assoc_id);
            break;
        }
        case SCTP_PARTIAL_DELIVERY_EVENT: {
            const struct sctp_pdapi_event *pdapi = &sn->sn_pdapi_event;
            n = snprintf(out, out_len,
                         "PARTIAL_DELIVERY_EVENT ind=%u stream=%u seq=%u assoc_id=%d",
                         pdapi->pdapi_indication,
                         pdapi->pdapi_stream,
                         pdapi->pdapi_seq,
                         pdapi->pdapi_assoc_id);
            break;
        }
        case SCTP_AUTHENTICATION_EVENT: {
            const struct sctp_authkey_event *auth = &sn->sn_authkey_event;
            n = snprintf(out, out_len,
                         "AUTHENTICATION_EVENT ind=%u keynumber=%u assoc_id=%d",
                         auth->auth_indication,
                         auth->auth_keynumber,
                         auth->auth_assoc_id);
            break;
        }
        case SCTP_SENDER_DRY_EVENT: {
            n = snprintf(out, out_len, "SENDER_DRY_EVENT assoc_id=%d",
                         sn->sn_sender_dry_event.sender_dry_assoc_id);
            break;
        }
        default: {
            n = snprintf(out, out_len, "UNHANDLED NOTIFICATION type=%u flags=%u len=%u",
                         sn->sn_header.sn_type,
                         sn->sn_header.sn_flags,
                         sn->sn_header.sn_length);
            break;
        }
    }

    if (n < 0) {
        return -1;
    }
    // snprintf returns what it would have written; report what it did write.
    return n < out_len ? n : out_len - 1;
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
