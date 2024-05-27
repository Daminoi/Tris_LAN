#ifndef MINILOGGER_H
#define MINILOGGER_H

#include "common.h"

typedef enum miniLogLevel{
    ERROR,
    WARNING,
    INFO,
    LOG
}miniLogLevel;

void mini_log(miniLogLevel log_level, const char* const function, const int line, const char* const txt);

#endif /* MINILOGGER_H */