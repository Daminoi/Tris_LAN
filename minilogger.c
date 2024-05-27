#include <stdio.h>
#include <string.h>

#include "minilogger.h"

void mini_log(miniLogLevel log_level, const char* const function, const int line, const char* const txt){
    #ifdef DEBUG
    char log_string[10];

    if(txt == NULL){
        return;
    }

    switch(log_level){
    case ERROR:
        snprintf(log_string, 6, "ERROR");
        break;
    case WARNING:
        snprintf(log_string, 8, "WARNING");
        break;
    case INFO:
        snprintf(log_string, 5, "INFO");
        break;
    case LOG:
        snprintf(log_string, 4, "LOG");
        break;
    default:
        snprintf(log_string, 10, "UNDEFINED");
        break;
    }

    if(function == NULL){
        printf("%s: %s\n", log_string, txt);
    }
    else if(function != NULL && line < 0){
        printf("%s (%s): %s\n", log_string, function, txt);
    }
    else{
        printf("%s (%s at line %d): %s\n", log_string, function, line, txt);
    }
    #endif

    return;
}

