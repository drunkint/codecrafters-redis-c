#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hash.h"
#include "timer.h"
#include "format.h"



// load factor = num of elements / table size
#define MIN_HASHTABLE_SIZE 4

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

bool is_hash_entry_valid(HashEntry* e) {
  return e != NULL && e->type != TYPE_NONE && (e->expiry_time == 0 || e->expiry_time > get_time_in_ms());
}

HashTable* ht_create_table(unsigned long table_size) {
  HashTable* new_ht = calloc(1, sizeof(HashTable));
  new_ht->num_of_elements = 0;
  new_ht->table_size = table_size < MIN_HASHTABLE_SIZE ? MIN_HASHTABLE_SIZE : table_size;
  
  HashEntry* entries = calloc(table_size, sizeof(HashEntry));
  for (int i = 0; i < table_size; i++) {
    entries[i].key = NULL;
    entries[i].value = NULL;
    entries[i].type = TYPE_NONE;
    entries[i].expiry_time = 0;
    entries[i].next = NULL;
  }

  new_ht->ht = entries;

  return new_ht;
}

void hash_entry_assign(HashEntry* dest, const char* key, const char* value, Type value_type, unsigned long expiry_time, HashEntry* next) {
  dest->key = calloc(1, strlen(key) + 1);
  strcpy(dest->key, key);

  dest->value = calloc(1, strlen(value) + 1);
  strcpy(dest->value, value);

  dest->type = value_type;
  dest->expiry_time = expiry_time;
  dest->next = next;
}

HashEntry* hash_entry_create(const char* key, const char* value, Type value_type, unsigned long expiry_time) {
  HashEntry* new_entry = calloc(1, sizeof(HashEntry));
  hash_entry_assign(new_entry, key, value, value_type, expiry_time, NULL);

  return new_entry;
}

void hash_entry_delete(HashEntry* hash_entry, bool should_free, HashEntry* prev) {
  if (prev != NULL) {
    prev->next = hash_entry->next;
  }
  printf("-- del start\n");
  printf("-- del key\n");

  free(hash_entry->key);
  hash_entry->key = NULL;

  printf("-- del val\n");
  free(hash_entry->value);
  hash_entry->value = NULL;

  printf("-- del other non pointer stuff\n");
  hash_entry->type = TYPE_NONE;
  hash_entry->expiry_time = 0;
  hash_entry->next = NULL;

  if (should_free) {
    printf("-- del free\n");
    free(hash_entry);
  }
}

// assumes hash_entry is not null, but might be empty.
bool hash_entry_should_be_set(HashEntry* hash_entry, const char* key) {
  return hash_entry != NULL && (                                           // hash entry is null
          hash_entry->key == NULL ||                                        // hash entry's key is empty
          (hash_entry->key != NULL && strcmp(hash_entry->key, key) == 0));   // hash entry's key matches provided key
}

unsigned long hash_func_djb2(const char* key, unsigned long table_size) {
  unsigned long hash_value = 5381;
  for (int i = 0; i < strlen(key); i++) {
    hash_value = ((hash_value << 5) + hash_value) + key[i]; /* hash * 33 + c */
  }
  return hash_value % table_size;
}

void ht_delete_table(HashTable* hash_table) {
  HashEntry* ht = hash_table->ht;
  for (int i = 0; i < hash_table->table_size; i++) {
    HashEntry* cur = &ht[i];
    HashEntry* next = cur->next;

    // delete the entry in the ht array (shouldn't free)
    hash_entry_delete(cur, false, NULL);
    cur = next;

    // delete the chained entries (should free)
    while (cur != NULL) {
      next = cur->next;
      printf("hii\n");
      hash_entry_delete(cur, true, NULL);
      cur = next;
    }
  }
  free(ht);
  free(hash_table);
}

// resize is either up or down.
// returns the address of the new table
HashTable* ht_resize(HashTable* src_ht, bool is_up) {
  HashTable* dest_ht;
  if (is_up) {
    dest_ht = ht_create_table(src_ht->table_size * (unsigned long) 2);
  } else {
    dest_ht = ht_create_table(src_ht->table_size / (unsigned long) 2);
  }

  HashEntry* ht = src_ht->ht;
  for (int i = 0; i < src_ht->table_size; i++) {
    if (is_hash_entry_valid(&ht[i])) {
      char type_temp[10] = {0};
      get_type(type_temp, ht[i].type);
      printf("-- adding %d: (%s, %s), type %s, expire %lu\n", i, ht[i].key, ht[i].value, type_temp, ht[i].expiry_time);
      ht_set(dest_ht, ht[i].key, ht[i].value, ht[i].type, ht[i].expiry_time);
      printf("--- added %d: (%s, %s), type %s, expire %lu\n", i, ht[i].key, ht[i].value, type_temp, ht[i].expiry_time);

    }
    HashEntry* cur = ht[i].next;
    while(cur != NULL) {
      if (!is_hash_entry_valid(&ht[i])) {
        cur = cur->next;
        continue;
      }
      char type_temp[10] = {0};
      get_type(type_temp, cur->type);
      printf("-> adding %d: (%s, %s), type %s, expire %lu\n", i, cur->key, cur->value, type_temp, cur->expiry_time);

      ht_set(dest_ht, cur->key, cur->value, cur->type, cur->expiry_time);
      printf("--> added %d: (%s, %s), type %s, expire %lu\n", i, cur->key, cur->value, type_temp, cur->expiry_time);

      cur = cur->next;
    }
  }
  
  printf("--- before delete\n");

  ht_delete_table(src_ht);
  printf("--- after delete\n");
  return dest_ht;
}

// upper limit: 3/4 of size or 1 of size
// lower limit: 1/8 of size
HashTable* ht_handle_resizing(HashTable* ht) {
  // unsigned long upper_limit = (ht->table_size / (unsigned long) 4) * (unsigned long) 3;
  unsigned long upper_limit = ht->table_size;

  unsigned long lower_limit = (ht->table_size / (unsigned long) 8);
  printf("- start of handling resizing, num of elem: %lu, upper: %lu, lower: %lu\n", ht->num_of_elements, upper_limit, lower_limit);

  if (ht->num_of_elements >= upper_limit) {
    return ht_resize(ht, true);
  } else if (ht->num_of_elements < lower_limit && ht->table_size > MIN_HASHTABLE_SIZE) {
    return ht_resize(ht, false);
  }

  return ht; // table didn't change
}

void ht_set(HashTable* ht, const char* key, const char* value, Type value_type, unsigned long expiry_time) {
  unsigned long index = hash_func_djb2(key, ht->table_size);
  HashEntry* cur = &ht->ht[index];
  HashEntry* prev = NULL;       // only used when chaining a new hash_entry

  while(cur != NULL) {
    if (hash_entry_should_be_set(cur, key)) {
      hash_entry_assign(cur, key, value, value_type, expiry_time, cur->next);
      ht->num_of_elements++;
      return;
    }

    prev = cur;       // only used when chaining a new hash_entry
    cur = cur->next;
  }

  // cur is NULL, prev is previous node. Should chain a new hash_entry
  cur = hash_entry_create(key, value, value_type, expiry_time);
  prev->next = cur;
  ht->num_of_elements++;
  return;
  
}

char* ht_get(HashTable* ht, const char* key) {
  unsigned long index = hash_func_djb2(key, ht->table_size);
  HashEntry* cur = &ht->ht[index];
  HashEntry* prev = NULL;
  bool is_first_in_chain = true;

  while(cur != NULL && cur->type != TYPE_NONE) {
    if (strcmp(cur->key, key) == 0 && is_hash_entry_valid(cur)) {
      return cur->value;
    } else if (strcmp(cur->key, key) == 0 && !is_hash_entry_valid(cur)) {

      hash_entry_delete(cur, !is_first_in_chain, prev);
      return NULL;
    }

    prev = cur;
    cur = cur->next;
    is_first_in_chain = false;
  }

  return NULL;
}

void ht_print(HashTable* hash_table) {
  HashEntry* ht = hash_table->ht;
  for (int i = 0; i < hash_table->table_size; i++) {
    char type_temp[10] = {0};
    if (ht[i].type != TYPE_NONE) {
      get_type(type_temp, ht[i].type);
      printf("%d: (%s, %s), type %s, expire %lu ", i, ht[i].key, ht[i].value, type_temp, ht[i].expiry_time);
    }
    HashEntry* cur = ht[i].next;
    while(cur != NULL) {
      if (cur->type == TYPE_NONE) {
        cur = cur->next;
        continue;
      }

      get_type(type_temp, cur->type);
      printf("-> %d: (%s, %s), type %s, expire %lu ", i, cur->key, cur->value, type_temp, cur->expiry_time);
      cur = cur->next;
    }
    if (ht[i].type != TYPE_NONE) {
      printf("\n");
    }
  }
}

// void delete_hash_entry(HashEntry* hash_entry) {
//   free(hash_entry->key);
//   free(hash_entry->value);

//   hash_entry->key = NULL;
//   hash_entry->value = NULL;
//   hash_entry->expiry_time = 0;
//   hash_entry->type = TYPE_NONE;
// }

// void hashtable_delete(HashEntry hash_table[], const char* key) {
//   for (int i = 0; i < HASH_NUM; i++) {
//     if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
//       delete_hash_entry(&hash_table[i]);
//       return;
//     }
//   }
// }

// // returns the index of the entry
// // only sets the (key, value) pair (No expiry_time)
// bool hashtable_set(HashEntry hash_table[], Type type, const char* key, const char* value, unsigned long expiry_time) {
//   // printf("in hashtable_set, expiry time is: %d\n", expiry_time);

//   for (int i = 0; i < HASH_NUM; i++) {

//     // setting an existing entry
//     if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0) {
//       if (value == NULL) {
//         hash_table[i].value = NULL;
//         hash_table[i].expiry_time = expiry_time;
//         hash_table[i].type = TYPE_NONE;
//       } else {
//         free(hash_table[i].value);
//         hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
//         strcpy(hash_table[i].value, value);

//         hash_table[i].expiry_time = expiry_time;
//         hash_table[i].type = TYPE_STRING;
//       }
//       return true;
//     }
//   }

//   // setting a blank entry
//   for (int i = 0; i < HASH_NUM; i++) {
//     if (hash_table[i].key == NULL) {
//       hash_table[i].key = calloc(strlen(key) + 1, sizeof(char));
//       strcpy(hash_table[i].key, key);

//       hash_table[i].value = calloc(strlen(value) + 1, sizeof(char));
//       strcpy(hash_table[i].value, value);

//       hash_table[i].expiry_time = expiry_time;
//       hash_table[i].type = TYPE_STRING;

//       return true;
//     }
//   }
//   return false;
// }

// int hashtable_find(HashEntry hash_table[], char* key) {
//   unsigned long current_time = get_time_in_ms();
  
//   for (int i = 0; i < HASH_NUM; i++) {
//     if (hash_table[i].key != NULL && strcmp(hash_table[i].key, key) == 0 ) { // found key
//       if (0 < hash_table[i].expiry_time  && hash_table[i].expiry_time <= current_time) { // expired
//         delete_hash_entry(&hash_table[i]);
//         return -1;
//       } 

//       // not expired
//       return i;
//     }
//   }
//   return -1;
// }

// char* hashtable_get(HashEntry hash_table[], char* key) {
//   int hash_entry_index = hashtable_find(hash_table, key);
//   if (hash_entry_index == -1) {
//     return NULL;
//   }
//   return hash_table[hash_entry_index].value;
// }

// // returns number of keys
// int hashtable_get_all_keys(HashEntry hash_table[], char result[][MAX_ARGUMENT_LENGTH]) {
//   int number_of_keys = 0;
//   for (int i = 0; i < HASH_NUM; i++) {
//     if (hash_table[i].key != NULL) {
//       strcpy(result[number_of_keys], hash_table[i].key);
//       number_of_keys++;
//     }
//   }
//   return number_of_keys;
// }

// void hashtable_get_type(HashEntry hash_table[], char* result, char* key) {
//   int hash_entry_index = hashtable_find(hash_table, key);
//   get_type(result, hash_table[hash_entry_index].type);
// }