// std
#include <stdint.h>
#include <time.h>
#include <string.h>

// src
#include <src/log.h>
#include <src/sctp.h>
#include <src/mmsg.h>

// tests
#include <sctp_events_test.h>

extern int IS_PROGRAM_RUNNING;

void _events_test(const char* host, uint16_t port, sctp_options_container_t socket_options_container)
{
    int sockfd = sctp_socket(host, port, socket_options_container);
    if (sockfd < 0) {
        LOG("error creating server socket");
        return;
    }

    if (sctp_listen(sockfd)) {
        LOG("error listening on server socket");
        return;
    }

    mmsg_t *mmsg = mmsg_create(10, 9216);
    time_t start = time(NULL);

    while(IS_PROGRAM_RUNNING) {
        int nmsg = mmsg_recv(mmsg, sockfd);
        mmsg_message_dump(mmsg, nmsg);
        if (time(NULL) - start > 3) break;
        usleep(0.5);
    }

    mmsg_destroy(&mmsg);
}

int sctp_events_test() {
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

    _events_test("0.0.0.0", 12346, socket_options_container);

    return 0;
}
