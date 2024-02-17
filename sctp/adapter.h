#pragma once

#include "sctp.h"
#include "mmsg.h"

// TODO: figure out error propagation and error handling.
int DefaultSctpServer(const char *ip, uint16_t port);
int DefaultSctpClient(const char *ip, uint16_t port);

// TODO: this API is unclear; mixture between msg and mmsg.
int SendMsg(int sockfd, const char* dst, int port, const char* buf, int len);
int RecvMsg(int sockfd, char* buf, int buf_len, struct sockaddr *saddr, unsigned int *saddr_len);
int SendMultiMsg(int sockfd, const char* dst, int port, mmsg_t *mmsg, int numMsg);
int RecvMultiMsg(int sockfd, mmsg_t *mmsg);

// TODO: decide whether to implement these or just use mmsg_* functions.
// The decision depends whether the adapter is actually considered important.
// Decoupling has benefits. For example, changes in the C implementation API
// will not influence code in go.
mmsg_t *CreateMmsg(int len, int bufsize);
void DestroyMmsg(mmsg_t **mmsg);
mmsg_iterator_t GetMmsgIterator(mmsg_t *mmsg);
int MmsgIteratorNext(mmsg_iterator_t *iterator);

int GetAddress(const struct sockaddr *saddr, char *buf, int buf_len, uint16_t *port);
