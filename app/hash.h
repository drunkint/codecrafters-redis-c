#include <time.h>
#include "format.h"

#define HASH_NUM 1021
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
	unsigned long expiry_time;
	Type type;

	// only used when type = TYPE_STREAM
} HashEntry;


void hashtable_init(HashEntry hash_table[]);
bool hashtable_set(HashEntry hash_table[], Type type, const char* key, const char* value, unsigned long expiry_time);
char* hashtable_get(HashEntry hash_table[], char* key);
void hashtable_print(HashEntry hash_table[]);
int hashtable_get_all_keys(HashEntry hash_table[], char result[][MAX_ARGUMENT_LENGTH]);
void hashtable_get_type(HashEntry hash_table[], char* result, char* key);