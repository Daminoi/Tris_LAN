#ifndef COMMUNICATION_H
#define COMMUNICATION_H

/* do not modify MESSAGE_QUEUE_SIZE */
#define MESSAGE_QUEUE_SIZE 2

#include "stdbool.h"
#include "protocol.h"
#include "common.h"

struct conn_status{
    bool terminated_by_game;
    bool terminated_by_conn_manager;
    bool terminated_by_other_peer;
};

bool validate_message(struct message* msg);

void copy_message(struct message* dest, struct message* src);

void* connection_manager();

void print_message(struct message* msg);

#endif /* COMMUNICATION_H */