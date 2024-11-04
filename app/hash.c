#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hash.h"
#include <string.h>

void hashtable_init(HashEntry hash_table[]) {
  for (int i = 0; i < HASH_NUM; i++) {
    hash_table[i].key = NULL;
    hash_table[i].value = NULL;
  }
}

bool hashtable_set(HashEntry hash_table[], const char* key, const char* value) {
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
      if (value == NULL) {
        hash_table[i].value = NULL;
      } else {
        free(hash_table[i].value);

        hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
        strcpy(hash_table[i].value, value);
      }
      return true;
    }
  }

  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key == NULL) {
      hash_table[i].key = calloc(strlen(key) + 1, sizeof(char));
      strcpy(hash_table[i].key, key);

      hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
      strcpy(hash_table[i].value, value);
      return true;
    }
  }
  return false;
}

char* hashtable_get(HashEntry hash_table[], char* key) {
  for (int i = 0; i < HASH_NUM; i++) {
    if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
      return hash_table[i].value;
    }
  }
  return NULL;
}



