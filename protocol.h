#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>

enum comm{
    OK = 0,
    NO_RESYNC = 1,
    NO_UNEXPECTED = 2,
    WELCOME = 3,
    DENIED = 4,
    DISCONNECT = 5,
    SET = 6,
    PLACE = 7,
    WIN = 8,
    SYNC_START = 9,
    SYNC_FINISCHED = 10
};

enum role{
    HOST = 1,
    GUEST = 2
};

enum phase{
    OPEN_CONNECTION,
    INITIAL_SYNC,
    RESYNC,
    GAME_TURN_GUEST,
    GAME_TURN_HOST,
    GAME_END,
    GAME_INTERRUPTED
};

struct gameState{
    enum phase phase;
    enum role role;
    enum comm last_comm;
};

struct message{
    enum comm communication;
    int n_args;
    int arg1;
    int arg2;
};

#endif /* PROTOCOL_H */