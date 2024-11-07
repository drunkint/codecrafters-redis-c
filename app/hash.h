#include <time.h>
#include "format.h"

#define HASH_NUM 1021

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
} HashEntry;


void hashtable_init(HashEntry hash_table[]);
bool hashtable_set(HashEntry hash_table[], const char* key, const char* value, unsigned long expiry_time);
char* hashtable_get(HashEntry hash_table[], char* key);
void hashtable_print(HashEntry hash_table[]);
int hashtable_get_all_keys(HashEntry hash_table[], char result[][MAX_ARGUMENT_LENGTH]);
bool hashtable_get_type(HashEntry hash_table[], char* result, char* key);