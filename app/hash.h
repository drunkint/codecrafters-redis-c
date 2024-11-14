#include <time.h>
#include "format.h"
#include "radix-trie.h"

// #define HASH_NUM 1021


#define STREAM_ID_SIZE 30
#define MAX_NUM_OF_KEYS_IN_STREAM 10

typedef enum {
	TYPE_STRING,
	TYPE_NONE,
	TYPE_STREAM
} Type;

typedef struct HashEntry {
	char* key;									// when type is a stream, this is a ID.
	char* value;
	unsigned long expiry_time;	// stored in milliseconds
	Type type;

	struct HashEntry* next;			// for hash chaining		
	bool is_first_in_chain;

	RadixNode* stream;								// only used when type is a stream
} HashEntry;

typedef struct HashTable {
	unsigned long table_size;
	unsigned long num_of_elements;

	struct HashEntry* ht;
} HashTable;

HashTable* ht_create_table(unsigned long table_size);
HashTable* ht_handle_resizing(HashTable* ht);
HashEntry* ht_set(HashTable* ht, const char* key, const char* value, Type value_type, unsigned long expiry_time);
HashEntry* ht_get_entry(HashTable* ht, const char* key);
Type ht_get_type(HashTable* ht, const char* key);
void get_type_string(char* result, Type type);
char* ht_get_value(HashTable* ht, const char* key);
char** ht_get_keys(const HashTable* hash_table, const char* pattern);
void ht_print(HashTable* hash_table);
bool ht_delete(HashTable* ht, const char* key);