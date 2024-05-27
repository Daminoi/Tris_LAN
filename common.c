#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "common.h"
#include "minilogger.h"

void get_current_time_in_timespec(struct timespec* timestamp){
    clock_gettime(CLOCK_REALTIME, timestamp);
}

/* Returns true if t1 is greater than t2*/
bool is_greater(struct timespec* t1, struct timespec* t2){
    if(t1 == NULL || t2 == NULL){
        mini_log(ERROR, "is_greater", -1, "cannot compare NULL timespecs!");
        return false;
    }

    if(t1->tv_sec > t2->tv_sec){
        return true;
    }
    else if(t1->tv_sec == t2->tv_sec){
        if(t1->tv_nsec > t2->tv_nsec){
            return true;
        }
        else{
            return false;
        }
    }
    else{
        return false;
    }
}

void ms_to_timespec(int ms, struct timespec* t){
    if(t == NULL){
        return;
    }

    if(ms < 0){
        ms = 0;
    }

    t->tv_sec = ms / 1000;
    t->tv_nsec = (ms % 1000) * 1000000;

    return;
}

/* The value of op1 is added to op2. op2 will contain the result of the sum.*/
void add_timespec(const struct timespec* op1, struct timespec* op2){
    if(op1 == NULL || op2 == NULL){
        return;
    }

    op2->tv_nsec += op1->tv_nsec;
    if(op2->tv_nsec > 1000000000){
        op2->tv_nsec -= 1000000000;
        op2->tv_sec += 1;
    }

    op2->tv_sec += op1->tv_sec;
}

/* the function puts the current time in absolute_time with an offset of ms milliseconds */
void get_absolute_time_with_offset(int ms, struct timespec* absolute_time){
    struct timespec ms_offset;

    get_current_time_in_timespec(absolute_time);
    ms_to_timespec(ms, &ms_offset);
    add_timespec(&ms_offset, absolute_time);
}

/* The caller is supended for at least ms milliseconds */
void ms_sleep(const int ms){
    if(ms < 0){
        return;
    }

    int tmp = 0;
    struct timespec time_left;

    ms_to_timespec(ms, &time_left);

    do {
        tmp = nanosleep(&time_left, &time_left);
    } while (tmp != 0 && errno == EINTR);

    return; 
}

void clean_console(){
    #ifndef DEBUG
    system("clear");
    #endif

    return;
}

void close_socket(int socket){
    if( close(socket) < 0){
        mini_log(ERROR, "close discovery socket", -1, "close socket failure");
    }
}

void wait_for_any_key_press(){
    fflush(stdin);
    getchar();
}