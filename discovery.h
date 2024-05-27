#ifndef DISCOVERY_H
#define DISCOVERY_H

#define DISCOVERY_PORT 49999

#include "common.h"

typedef struct discoveryMesssage{
    int version;
    int tcp_port;       // specifies the tcp port that guest can use to join a game
} discoveryMesssage;

void* discovery();

void prepare_discovery_message(discoveryMesssage* msg, int tcp_port);

#endif /* DISCOVERY_H */