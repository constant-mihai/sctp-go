#include "log.h"
#include "sctp.h"
#include "mmsg.h"
#include <pthread.h>
#include <signal.h>

typedef struct {
    const char *host;
    uint16_t port;
    sctp_options_container_t socket_options_container;

    int retval;
} sctp_thread_t;

typedef void*(thread_handler_f)(void*);

typedef struct {
    pthread_t id;
    thread_handler_f *handler;
    sctp_thread_t args;
} thread_declaration_t;

static int IS_PROGRAM_RUNNING;

void *server_thread(void *arg)
{
    int sockfd;

    sctp_thread_t *server_args = (sctp_thread_t*)arg;
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

    while(IS_PROGRAM_RUNNING) {
        int nmsg = mmsg_recv(mmsg, sockfd);
        mmsg_message_dump(mmsg, nmsg);
    }

    mmsg_destroy(&mmsg);
    server_args->retval = 0;
    return arg;
}

int mmsg_init_test_message(mmsg_t *mmsg, struct sockaddr *saddr, int saddr_len) {
    int nmsg = 5;
    for (size_t i = 0; i < nmsg; i++) {
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

void *client_thread(void *arg)
{
    int sockfd;

    sctp_thread_t *client_args = (sctp_thread_t*)arg;
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
    int nmsg = mmsg_init_test_message(mmsg, &saddr, saddr_len);
    while(IS_PROGRAM_RUNNING) {
        //mmsg_message_dump(mmsg, nmsg);
        mmsg_send(mmsg, nmsg, sockfd);
        sleep(100);
    }

    mmsg_destroy(&mmsg);
    client_args->retval = 0;
    return arg;
}

void signal_handler(int signum) {
    LOG("Received signal: %d, stopping program", signum);
    IS_PROGRAM_RUNNING = 0;
}

void install_signal_handlers() {
    struct sigaction sa = { .sa_handler = signal_handler };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    IS_PROGRAM_RUNNING = 1;
}

void start_thread(thread_declaration_t *thread) {
    int status;
    status = pthread_create(&thread->id, NULL, thread->handler, (void*)&thread->args);
    if (status != 0) { 
        LOG("error starting thread: %d", strerror(status));
        exit(1);
    }
}

void join_threads(pthread_t id) {
    void *thread_result;
    int status;
    status = pthread_join(id, &thread_result);
    if (status != 0) {
        LOG("error joining thread: %d", strerror(status));
        exit(1);
    }

    sctp_thread_t *arg = (sctp_thread_t*) thread_result;
    LOG("thread retval: %d", arg->retval);
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    LOG("main");

    install_signal_handlers();

    int toggle_on = 1;
    int toggle_off = 0;

    struct sctp_event_subscribe events;
    memset(&events, 0, sizeof(struct sctp_event_subscribe));
    events.sctp_data_io_event = 1;
    events.sctp_association_event = 1;
    events.sctp_address_event = 1;
    events.sctp_send_failure_event = 1;
    events.sctp_peer_error_event = 1;
    events.sctp_shutdown_event = 1;

    sctp_option_t socket_options[] = {
        //{ .fn = sctp_option_set_nonblocking, .args = &toggle_on },
        { .fn = sctp_option_set_interleave, .args = &toggle_off },
        { .fn = sctp_option_set_partial_delivery_point, .args = &toggle_off },
        //{ .fn = sctp_option_subscribe_to_events, .args = &events},
    };
    sctp_options_container_t socket_options_container = {
        .options = socket_options,
        .len = sizeof(socket_options)/sizeof(socket_options[0]),
    };
    thread_declaration_t threads[2] = {
        {
            .handler = server_thread,
            .args = {
                .host = "0.0.0.0", .port = 12345,
                .socket_options_container = socket_options_container,
            },
        },
        {
            .handler = client_thread,
            .args = {
                .host = "0.0.0.0", .port = 54321,
                .socket_options_container = socket_options_container,
            },
        },
    };
    
    for (size_t i=0; i<sizeof(threads)/sizeof(threads[0]); i++) {
        start_thread(&threads[i]);
        usleep(0.5);
    }
    
    for (size_t i=0; i<sizeof(threads)/sizeof(threads[0]); i++) {
        join_threads(threads[i].id);
    }

    LOG("terminating");
    fflush(stdout);
    return 0;
}
