#include <src/log.h>
#include <src/sctp.h>
#include <src/mmsg.h>

#include <src/thread.h>

extern int IS_PROGRAM_RUNNING;
int CLIENT_IS_DONE;

typedef struct {
    const char *host;
    uint16_t port;
    sctp_options_container_t socket_options_container;

    int messages_sent;
    int messages_received;

    int bytes_sent;
    int bytes_received;

    int retval;
} sctp_thread_args_t;

void *_server_thread(void *arg)
{
    int sockfd;

    sctp_thread_args_t *server_args = (sctp_thread_args_t*)arg;
    sockfd = sctp_socket(server_args->host,
                         server_args->port,
                         server_args->socket_options_container);
    if (sockfd < 0) {
        LOG("error creating server socket");
        server_args->retval = -1;
        return arg;
    }

    if (sctp_listen(sockfd)) {
        LOG("error listening on server socket");
        server_args->retval = -1;
        return arg;
    }

    mmsg_t *mmsg = mmsg_create(10, 9216);

    // TODO: test bi-directional traffic
    while(IS_PROGRAM_RUNNING && !CLIENT_IS_DONE) {
        int nmsg = mmsg_recv(mmsg, sockfd);
        mmsg_message_dump(mmsg, nmsg);
        server_args->messages_received += nmsg;
    }

    mmsg_destroy(&mmsg);
    server_args->retval = 0;
    return arg;
}

int _init_test_message(mmsg_t *mmsg, struct sockaddr *saddr, int saddr_len) {
    int nmsg = 10;
    for (int i = 0; i < nmsg; i++) {
        struct msghdr *msg_hdr = &mmsg->vec[i].msg_hdr;
        memcpy(msg_hdr->msg_iov->iov_base, "test", 4);
        msg_hdr->msg_iov->iov_len = 4;
        msg_hdr->msg_iovlen = 1;

        /* set the remote address */
        msg_hdr->msg_name = (void*)saddr;
        msg_hdr->msg_namelen = saddr_len;

        /* initialize specific SCTP control message header */
        //msg_hdr->msg_controllen=CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
        //cmsg=CMSG_FIRSTHDR(msg_hdr);
        //sinfo=(struct sctp_sndrcvinfo*)CMSG_DATA(cmsg);
    }
    return nmsg;
}

void *_client_thread(void *arg)
{
    int sockfd;

    sctp_thread_args_t *client_args = (sctp_thread_args_t*)arg;
    sockfd = sctp_socket(client_args->host,
                         client_args->port,
                         client_args->socket_options_container);
    if (sockfd < 0) {
        LOG("error creating client socket");
        client_args->retval = -1;
        return arg;
    }

    mmsg_t *mmsg = mmsg_create(10, 9216);

    struct sockaddr saddr;
    int saddr_len;
    if (sctp_get_saddr(AF_INET, "127.0.0.1", 12345,
                       &saddr, &saddr_len)) {
        exit(1);
    }
    // TODO: add timeout; if the client has problems sending,
    // this will loop infinitely.
    int nmsg = _init_test_message(mmsg, &saddr, saddr_len);
    while(IS_PROGRAM_RUNNING && client_args->messages_sent < 100) {
        LOG("Sending %d messages", nmsg);
        //mmsg_message_dump(mmsg, nmsg);
        client_args->messages_sent += mmsg_send(mmsg, nmsg, sockfd);
        usleep(0.5);
    }
    CLIENT_IS_DONE = 1;
    LOG("Client is done");

    mmsg_destroy(&mmsg);
    client_args->retval = 0;
    return arg;
}

int sctp_test_send() {

    int toggle_off = 0;
    CLIENT_IS_DONE = 0;

    sctp_option_t socket_options[] = {
        { .fn = sctp_option_set_interleave, .args = &toggle_off },
        { .fn = sctp_option_set_partial_delivery_point, .args = &toggle_off },
        //{ .fn = sctp_option_subscribe_to_events, .args = &events},
    };
    sctp_options_container_t socket_options_container = {
        .options = socket_options,
        .len = sizeof(socket_options)/sizeof(socket_options[0]),
    };
    sctp_thread_args_t sctp_server_args = {
        .host = "0.0.0.0", .port = 12345,
        .socket_options_container = socket_options_container,
        .messages_sent = 0,
        .messages_received = 0,
        .bytes_sent = 0,
        .bytes_received = 0,
    };
    sctp_thread_args_t sctp_client_args = {
        .host = "0.0.0.0", .port = 54321,
        .socket_options_container = socket_options_container,
        .messages_sent = 0,
        .messages_received = 0,
        .bytes_sent = 0,
        .bytes_received = 0,
    };
    thread_declaration_t threads[2] = {
        {
            .handler = _server_thread,
            .args = &sctp_server_args,
        },
        {
            .handler = _client_thread,
            .args = &sctp_client_args,
        },
    };
    
    for (size_t i=0; i<sizeof(threads)/sizeof(threads[0]); i++) {
        start_thread(&threads[i]);
        // Wait for the server to start.
        // Otherwise I need to complicate the code with thread barriers.
        sleep(1);
    }
    
    for (size_t i=0; i<sizeof(threads)/sizeof(threads[0]); i++) {
        join_thread(threads[i].id);
    }

    if (sctp_client_args.messages_sent == sctp_server_args.messages_received) {
        LOG("Client sent %d messages, server received: %d",
            sctp_client_args.messages_sent, 
            sctp_client_args.messages_received);
        return -1;
    }

    return 0;
}

// TODO: refactor this test with it's own handler functions.
int sctp_test_events() {
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
    sctp_thread_args_t sctp_server_args = {
        .host = "0.0.0.0", .port = 12346,
        .socket_options_container = socket_options_container,
    };
    sctp_thread_args_t sctp_client_args = {
        .host = "0.0.0.0", .port = 64321,
        .socket_options_container = socket_options_container,
    };
    thread_declaration_t threads[2] = {
        {
            .handler = _server_thread,
            .args = &sctp_server_args,
        },
        {
            .handler = _client_thread,
            .args = &sctp_client_args,
        },
    };
    
    for (size_t i=0; i<sizeof(threads)/sizeof(threads[0]); i++) {
        start_thread(&threads[i]);
        usleep(0.5);
    }
    
    for (size_t i=0; i<sizeof(threads)/sizeof(threads[0]); i++) {
        join_thread(threads[i].id);
    }

    return 0;
}
