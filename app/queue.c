#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "timer.h"

#define pass (void)0

Queue* q_init() {
  Queue* q = calloc(1, sizeof(Queue));
  q->head = NULL;

  return q;
}


void q_add(Queue* q, char decoded_command[][MAX_ARGUMENT_LENGTH], int num_of_arg, unsigned long expiry_time, int fd_index) {
  // creating the queue node
  Event* e = calloc(1, sizeof(Event));
  
  char** command = calloc(num_of_arg, sizeof(char*));
  for (int i = 0; i < num_of_arg; i++) {
    command[i] = calloc(strlen(decoded_command[i]) + 1, sizeof(char));
    strcpy(command[i], decoded_command[i]);
  }

  e->command = command;
  e->command_length = num_of_arg;
  e->expiry_time = expiry_time;
  e->fd_index = fd_index;

  // inserting the e into the queue
  // 0 items in the Queue
  if (q->head == NULL) {
    q->head = e;
    return;
  } 
  
  // >= 1 items in the Queue
  // insert into queue such that the queue is sorted by expiry time in ascending order
  Event* cur = q->head;
  Event* prev = NULL;
  while (cur != NULL && cur->expiry_time <= e->expiry_time) {
    prev = cur;
    cur = cur->next;
  }

  // inserted at first slot
  if (prev == NULL) {
    e->next = cur;
    q->head = e;
  } else {  // inserted at other slots
    prev->next = e;
    e->next = cur;
  }
  
}

Event* q_pop_front(Queue* q) {
  // cannot pop an empty queue
  if (q->head == NULL) {
    return NULL;
  }

  Event* item_to_pop = q->head;
  // >= 1 items in the queue before popping
  q->head = q->head->next;

  item_to_pop->next = NULL;
  return item_to_pop;

}

void q_destroy_event(Event* e) {
  for (int i = 0; i < e->command_length; i++) {
    // printf("- freeing command[%d] %s\n", i, e->command[i]);
    free(e->command[i]);
    
  }
  // printf("- freeing e->command\n");
  free(e->command);
  // printf("- freeing e\n");
  free(e);
}

void q_destroy_queue_without_freeing_q(Queue* q) {
  if (q == NULL || q->head == NULL) {
    return;
  }
  Event* cur = q->head;
  while (cur != NULL) {
    Event* next = cur->next;
    q_destroy_event(cur);
    cur = next;
  }
}

void q_destroy_queue(Queue* q) {
  q_destroy_queue_without_freeing_q(q);
  q == NULL ? pass : free(q);
}


bool q_is_head_expired(Queue* q) {
  if (q == NULL || q->head == NULL) {
    return false;
  }
  return q->head->expiry_time < get_time_in_ms();
}


void q_prepend(Queue* q, Event* e) {
  if (q == NULL) {
    q = q_init();
    q->head = e;
    return;
  }

  if (q->head == NULL) {
    q->head = e;
    return;
  }

  e->next = q->head;
  q->head = e;
  return;
}

Event* q_find_and_pop(Queue* q, char* command_keyword) {
  if (q == NULL || q->head == NULL) {
    return NULL;
  }

  Event* cur = q->head;
  Event* prev = NULL;
  while (cur != NULL) {
    // support the 2nd arg for now (third part)
    if (strcmp(cur->command[2], command_keyword) == 0) {
      if (prev != NULL) {
        prev->next = cur->next;
      } else {
        q->head = cur->next;
      }

      cur->next = NULL;
      return cur;
    }

    prev = cur;
    cur = cur->next;
  }

  return NULL;
}