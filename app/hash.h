#include <time.h>
#include "format.h"

// #define HASH_NUM 1021


#define STREAM_ID_SIZE 30
#define MAX_NUM_OF_KEYS_IN_STREAM 10

typedef enum {
	TYPE_STRING,
	TYPE_NONE,
	TYPE_STREAM
} Type;

typedef struct HashEntry {
	char* key;
	char* value;
	unsigned long expiry_time;	// stored in milliseconds
	Type type;

	struct HashEntry* next;			// for hash chaining		
} HashEntry;

typedef struct HashTable {
	unsigned long table_size;
	unsigned long num_of_elements;

	struct HashEntry* ht;
} HashTable;

HashTable* ht_create_table(unsigned long table_size);
void ht_set(HashTable* ht, const char* key, const char* value, Type value_type, unsigned long expiry_time);
char* ht_get(HashTable* ht, const char* key);
void ht_print(HashTable* hash_table);
HashTable* ht_handle_resizing(HashTable* ht);

// void hashtable_init(HashEntry hash_table[]);
// bool hashtable_set(HashEntry hash_table[], Type type, const char* key, const char* value, unsigned long expiry_time);
// char* hashtable_get(HashEntry hash_table[], char* key);
// void hashtable_print(HashEntry hash_table[]);
// int hashtable_get_all_keys(HashEntry hash_table[], char result[][MAX_ARGUMENT_LENGTH]);
// void hashtable_get_type(HashEntry hash_table[], char* result, char* key);