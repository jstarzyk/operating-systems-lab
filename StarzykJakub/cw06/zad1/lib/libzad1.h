#ifndef ZAD1_ZAD1_LIB_H
#define ZAD1_ZAD1_LIB_H

#include <stdio.h>
#include <sys/msg.h>

#include "../common.h"

const size_t message_data_size;

void send_message2(int queue_id, int flags, long type, pid_t pid, char *data, double result, int id);
Message *wait_for_message(int queue_id, long type, int flags);
key_t get_key(const char *path, char id);
int get_queue(int role, int flags, const char *path);
void remove_queue();
void handle_sigint(int signo);

#endif //ZAD1_ZAD1_LIB_H
