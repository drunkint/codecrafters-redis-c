#include "format.h"



typedef struct Event {
  struct Event* next;
  char** command;             // this shouldn't include block
  int command_length;
  unsigned long expiry_time;
  int fd_index;               // > 0 if client is waiting on fd[fd_index]. -1 if no client is waiting.
} Event;

typedef struct Queue {
  struct Event* head;
} Queue;

Queue* q_init();
void q_add(Queue* q, char decoded_command[][MAX_ARGUMENT_LENGTH], int num_of_arg, unsigned long expiry_time, int fd_index);
Event* q_pop_front(Queue* q);
void q_destroy_event(Event* e);
bool q_is_head_expired(Queue* q);