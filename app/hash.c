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
    
    printf("%s: (%s, %s), expire at: %lu\n", hash_table[i].type, hash_table[i].key, hash_table[i].value, hash_table[i].expiry_time);
  }
}

void get_type(char* result, Type type) {
  switch (type)
  {
  case TYPE_STREAM:
    strcpy(result, "stream");
    break;
  case TYPE_STRING:
    strcpy(result, "string");
    break;
  default:
    strcpy(result, "none");
    break;
  }
}

void delete_hash_entry(HashEntry* hash_entry) {
  free(hash_entry->key);
  free(hash_entry->value);

  hash_entry->key = NULL;
  hash_entry->value = NULL;
  hash_entry->expiry_time = 0;
  hash_entry->type = TYPE_NONE;
}

void hashtable_init(HashEntry hash_table[]) {
  for (int i = 0; i < HASH_NUM; i++) {
    hash_table[i].key = NULL;
    hash_table[i].value = NULL;
    hash_table[i].expiry_time = 0;
    hash_table[i].type = TYPE_NONE;
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
bool hashtable_set(HashEntry hash_table[], Type type, const char* key, const char* value, unsigned long expiry_time) {
  // printf("in hashtable_set, expiry time is: %d\n", expiry_time);

  for (int i = 0; i < HASH_NUM; i++) {

    // setting an existing entry
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
      if (value == NULL) {
        hash_table[i].value = NULL;
        hash_table[i].expiry_time = expiry_time;
        hash_table[i].type = TYPE_NONE;
      } else {
        free(hash_table[i].value);
        hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
        strcpy(hash_table[i].value, value);

        hash_table[i].expiry_time = expiry_time;
        hash_table[i].type = TYPE_STRING;
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
      hash_table[i].type = TYPE_STRING;

      return true;
    }
  }
  return false;
}

int hashtable_find(HashEntry hash_table[], char* key) {
  unsigned long current_time = get_time_in_ms();
  
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0 ) { // found key
      if (0 < hash_table[i].expiry_time  && hash_table[i].expiry_time <= current_time) { // expired
        delete_hash_entry(&hash_table[i]);
        return NULL;
      } 

      // not expired
      return i;
    }
  }
  return -1;
}

char* hashtable_get(HashEntry hash_table[], char* key) {
  int hash_entry_index = hashtable_find(hash_table, key);
  if (hash_entry_index == -1) {
    return NULL;
  }
  return hash_table[hash_entry_index].value;
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

void hashtable_get_type(HashEntry hash_table[], char* result, char* key) {
  int hash_entry_index = hashtable_find(hash_table, key);
  get_type(result, hash_table[hash_entry_index].type);
}