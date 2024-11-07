#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hash.h"
#include "timer.h"
#include "format.h"

void hashtable_print(HashEntry hash_table[]) {
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key == NULL) {
      continue;
    }
    
    printf("(%s, %s), expire at: %lu\n", hash_table[i].key, hash_table[i].value, hash_table[i].expiry_time);
  }
}

void delete_hash_entry(HashEntry* hash_entry) {
  free(hash_entry->key);
  free(hash_entry->value);

  hash_entry->key = NULL;
  hash_entry->value = NULL;
  hash_entry->expiry_time = 0;
}

void hashtable_init(HashEntry hash_table[]) {
  for (int i = 0; i < HASH_NUM; i++) {
    hash_table[i].key = NULL;
    hash_table[i].value = NULL;
    hash_table[i].expiry_time = 0;
  }
}

void hashtable_delete(HashEntry hash_table[], const char* key) {
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
      delete_hash_entry(&hash_table[i]);
      return;
    }
  }
}

// returns the index of the entry
// only sets the (key, value) pair (No expiry_time)
bool hashtable_set(HashEntry hash_table[], const char* key, const char* value, unsigned long expiry_time) {
  // printf("in hashtable_set, expiry time is: %d\n", expiry_time);

  for (int i = 0; i < HASH_NUM; i++) {

    // setting an existing entry
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
      if (value == NULL) {
        hash_table[i].value = NULL;
        hash_table[i].expiry_time = expiry_time;
      } else {
        free(hash_table[i].value);
        hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
        strcpy(hash_table[i].value, value);

        hash_table[i].expiry_time = expiry_time;
      }
      return true;
    }
  }

  // setting a blank entry
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key == NULL) {
      hash_table[i].key = calloc(strlen(key) + 1, sizeof(char));
      strcpy(hash_table[i].key, key);

      hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
      strcpy(hash_table[i].value, value);

      hash_table[i].expiry_time = expiry_time;

      return true;
    }
  }
  return false;
}

char* hashtable_get(HashEntry hash_table[], char* key) {
  unsigned long current_time = get_time_in_ms();
  
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0 ) { // found key
      if (0 < hash_table[i].expiry_time  && hash_table[i].expiry_time <= current_time) { // expired
        delete_hash_entry(&hash_table[i]);
        return NULL;
      } 

      printf("hit: (key, value): %s, %s\n", hash_table[i].key, hash_table[i].value);

      // not expired
      return hash_table[i].value;
    }
  }
  return NULL;
}

// returns number of keys
int hashtable_get_all_keys(HashEntry hash_table[], char result[][MAX_ARGUMENT_LENGTH]) {
  int number_of_keys = 0;
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key != NULL) {
      strcpy(result[number_of_keys], hash_table[i].key);
      number_of_keys++;
    }
  }
  return number_of_keys;
}

bool hashtable_get_type(HashEntry hash_table[], char* result, char* key) {
  char* value = hashtable_get(hash_table, key);
  if (value == NULL) {
    strcpy(result, TYPE_NONE);
  } else {
    strcpy(result, TYPE_STRING);
  }
}