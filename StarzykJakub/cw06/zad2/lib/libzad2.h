#ifndef ZAD2_ZAD2_LIB_H
#define ZAD2_ZAD2_LIB_H

#include "../common.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <mqueue.h>
#include <string.h>

const size_t message_data_size;

void send_message2(int queue_id, long type, pid_t pid, char *data, double result, int id);
Message *wait_for_message(int queue_id);
size_t get_pid_max_len();
char *get_queue_name(size_t pid_max_len);
int get_queue(const char *queue_name, int flags, mode_t mode);
void remove_queue();
void handle_sigint(int signo);

#endif //ZAD1_ZAD1_LIB_H
