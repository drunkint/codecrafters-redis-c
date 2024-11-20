#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hash.h"
#include "timer.h"
#include "format.h"



// load factor = num of elements / table size
#define MIN_HASHTABLE_SIZE 4

void get_type_string(char* result, Type type) {
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
    entries[i].is_first_in_chain = true;
  }

  new_ht->ht = entries;

  return new_ht;
}

void hash_entry_assign(HashEntry* dest, const char* key, const char* value, Type value_type, unsigned long expiry_time, HashEntry* next, bool is_first_in_chain) {
  dest->key = calloc(1, strlen(key) + 1);
  strcpy(dest->key, key);

  dest->value = calloc(1, strlen(value) + 1);
  strcpy(dest->value, value);

  dest->type = value_type;
  dest->expiry_time = expiry_time;
  dest->next = next;
  dest->is_first_in_chain = is_first_in_chain;
}

HashEntry* hash_entry_create(const char* key, const char* value, Type value_type, unsigned long expiry_time) {
  HashEntry* new_entry = calloc(1, sizeof(HashEntry));
  hash_entry_assign(new_entry, key, value, value_type, expiry_time, NULL, false);

  return new_entry;
}


bool key_should_be_replaced_in_existing_hash_entry(HashEntry* hash_entry, const char* key) {
  return hash_entry != NULL &&                                             // hash entry is null
         (hash_entry->key != NULL && strcmp(hash_entry->key, key) == 0);   // hash entry's key matches provided key
}


bool key_should_be_added_to_existing_hash_entry(HashEntry* hash_entry, const char* key) {
  return hash_entry != NULL &&                                           // hash entry is null
         hash_entry->key == NULL;                                        // hash entry's key is empty
}



unsigned long hash_func_djb2(const char* key, unsigned long table_size) {
  unsigned long hash_value = 5381;
  for (int i = 0; i < strlen(key); i++) {
    hash_value = ((hash_value << 5) + hash_value) + key[i]; /* hash * 33 + c */
  }
  return hash_value % table_size;
}

void hash_entry_free(HashEntry* hash_entry) {
  printf("- free start\n");
  printf("-- free key\n");

  free(hash_entry->key);
  hash_entry->key = NULL;

  printf("-- free val\n");
  free(hash_entry->value);
  hash_entry->value = NULL;

  hash_entry->next = NULL;
  hash_entry->type = TYPE_NONE;

  if (!hash_entry->is_first_in_chain) {
    printf("-- free whole entry\n");
    free(hash_entry);
  }
}

void ht_delete_table(HashTable* hash_table) {
  HashEntry* ht = hash_table->ht;
  for (int i = 0; i < hash_table->table_size; i++) {
    HashEntry* cur = &ht[i];
    HashEntry* next = cur->next;

    hash_entry_free(cur);
    cur = next;

    while (cur != NULL) {
      next = cur->next;
      printf("hii\n");
      hash_entry_free(cur);
      cur = next;
    }
  }
  free(ht);
  free(hash_table);
}

bool ht_delete(HashTable* ht, const char* key) {
  unsigned long index = hash_func_djb2(key, ht->table_size);
  HashEntry* cur = &ht->ht[index];
  HashEntry* prev = NULL;

  while(cur != NULL && cur->type != TYPE_NONE) {
    if (strcmp(cur->key, key) == 0) {
      // delete cur
      HashEntry* next = cur->next;
      // hash_entry_free(cur);
      if (cur->is_first_in_chain && next == NULL) {
        hash_entry_free(cur);
      } else if (cur->is_first_in_chain && next != NULL) {
        hash_entry_free(cur);
        hash_entry_assign(cur, next->key, next->value, next->type, next->expiry_time, next->next, true);
        hash_entry_free(next);
      } else {
        prev->next = next;
        hash_entry_free(cur);
      }

      ht->num_of_elements--;
      return true;
    }

    prev = cur;
    cur = cur->next;
  }

  return false;
  
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
      get_type_string(type_temp, ht[i].type);
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
      get_type_string(type_temp, cur->type);
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

// upper limit: 1 of size
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

// returns the newly created hash entry
HashEntry* ht_set(HashTable* ht, const char* key, const char* value, Type value_type, unsigned long expiry_time) {
  unsigned long index = hash_func_djb2(key, ht->table_size);
  HashEntry* cur = &ht->ht[index];
  HashEntry* prev = NULL;       // only used when chaining a new hash_entry
  bool is_first_in_chain = true;

  while(cur != NULL) {
    if (key_should_be_added_to_existing_hash_entry(cur, key)) {
      hash_entry_assign(cur, key, value, value_type, expiry_time, cur->next, is_first_in_chain);
      ht->num_of_elements++;
      return cur;
    }

    if (key_should_be_replaced_in_existing_hash_entry(cur, key)) {
      hash_entry_assign(cur, key, value, value_type, expiry_time, cur->next, is_first_in_chain);
      // printf("%s vs %s\n", cur->key, key);
      // printf("-num of elem: %d\n", ht->num_of_elements);
      return cur;
    }

    prev = cur;       // only used when chaining a new hash_entry
    cur = cur->next;
    is_first_in_chain = false;
  }

  // cur is NULL, prev is previous node. Should chain a new hash_entry
  cur = hash_entry_create(key, value, value_type, expiry_time);
  prev->next = cur;
  ht->num_of_elements++;
  // printf("--num of elem: %d\n", ht->num_of_elements);

  return cur;
  
}

HashEntry* ht_get_entry_ignore_expiry(HashTable* ht, const char* key) {
  unsigned long index = hash_func_djb2(key, ht->table_size);
  HashEntry* cur = &ht->ht[index];

  while(cur != NULL && cur->type != TYPE_NONE) {
    // printf("cur.key, key = %s, %s\n", cur->key, key);
    if (strcmp(cur->key, key) == 0) {
      return cur;
    }

    cur = cur->next;
  }
  return NULL;
}

HashEntry* ht_get_entry(HashTable* ht, const char* key) {
  HashEntry* e = ht_get_entry_ignore_expiry(ht, key);
  if (e == NULL) {
    return NULL;
  } else if (e->expiry_time > 0 && e->expiry_time <= get_time_in_ms()) {
    // delete entry
    ht_delete(ht, key);
    return NULL;
  }

  return e;
}

char* ht_get_value(HashTable* ht, const char* key) {
  HashEntry* e = ht_get_entry(ht, key);
  
  if (e == NULL) {
    return NULL;
  }

  return e->value;
}

Type ht_get_type(HashTable* ht, const char* key) {
  HashEntry* e = ht_get_entry(ht, key);
  
  if (e == NULL) {
    return TYPE_NONE;
  }

  return e->type;
}

void ht_print(HashTable* hash_table) {
  printf("printing hash table...\n");
  HashEntry* ht = hash_table->ht;
  for (int i = 0; i < hash_table->table_size; i++) {
    char type_temp[10] = {0};
    if (ht[i].type != TYPE_NONE) {
      get_type_string(type_temp, ht[i].type);
      printf("%d: (%s, %s), type %s, expire %lu ", i, ht[i].key, ht[i].value, type_temp, ht[i].expiry_time);
      if (ht[i].type == TYPE_STREAM) {
        rn_print(ht[i].stream);
      }
    }
    HashEntry* cur = ht[i].next;
    while(cur != NULL) {
      if (cur->type == TYPE_NONE) {
        cur = cur->next;
        continue;
      }

      get_type_string(type_temp, cur->type);
      printf("-> %d: (%s, %s), type %s, expire %lu ", i, cur->key, cur->value, type_temp, cur->expiry_time);
      if (ht[i].type == TYPE_STREAM) {
        rn_print(ht[i].stream);
      }
      cur = cur->next;
    }
    if (ht[i].type != TYPE_NONE) {
      printf("\n");
    }
  }
}

char** ht_get_keys(const HashTable* hash_table, const char* pattern) {
  HashEntry* ht = hash_table->ht;

  char** result = calloc(hash_table->num_of_elements, sizeof(char*));
  int result_counter = 0;
  for (int i = 0; i < hash_table->table_size; i++) {
    char type_temp[10] = {0};
    if (is_hash_entry_valid(&ht[i])) {
      // printf("-- adding %s\n", ht[i].key);
      result[result_counter] = calloc(strlen(ht[i].key) + 1, sizeof(char));
      strcpy(result[result_counter], ht[i].key);
      // printf("-- added %s\n", ht[i].key);

      result_counter++;
    }

    HashEntry* cur = ht[i].next;
    while(cur != NULL) {
      if (!is_hash_entry_valid(cur)) {
        cur = cur->next;
        continue;
      }

      printf("added %s\n", cur->key);
      result[result_counter] = calloc(strlen(cur->key) + 1, sizeof(char));
      strcpy(result[result_counter], cur->key);
      result_counter++;
      cur = cur->next;
    }
  }

  return result;
}
