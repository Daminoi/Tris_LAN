#ifndef COMMON_H
#define COMMON_H

/* comment this define to remove all logs */
//#define DEBUG

#include <time.h>
#include <stdbool.h>

void get_current_time_in_timespec(struct timespec* timestamp);

bool is_greater(struct timespec* t1, struct timespec* t2);

void ms_to_timespec(int ms, struct timespec* t);

void ms_sleep(const int ms);

void add_timespec(const struct timespec* op1, struct timespec* op2);

void get_absolute_time_with_offset(int ms, struct timespec* absolute_time);

void clean_console();

void close_socket(int socket);

void wait_for_any_key_press();

#endif /* COMMON_H */