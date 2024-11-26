#include "format.h"



typedef struct Event {
  struct Event* next;
  char** command;             // this shouldn't include block
  int command_length;
  unsigned long expiry_time;  // this is set to ULONG MAX when waiting on some other action.
  int fd_index;               // > 0 if client is waiting on fd[fd_index]. -1 if no client is waiting.
} Event;

typedef struct Queue {
  struct Event* head;
} Queue;

Queue* q_init();
void q_add(Queue* q, char decoded_command[][MAX_ARGUMENT_LENGTH], int num_of_arg, unsigned long expiry_time, int fd_index);
void q_destroy_event(Event* e);
void q_destroy_queue_without_freeing_q(Queue* q);
void q_destroy_queue(Queue* q);
void q_prepend(Queue* q, Event* e);

// used in event queue
Event* q_pop_front(Queue* q);
bool q_is_head_expired(Queue* q);

// used in trigger queue
Event* q_find_and_pop(Queue*q, char* command_keyword);     // find in command[2]