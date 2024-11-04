#include <time.h>

#define HASH_NUM 1021

typedef struct HashEntry {
	char* key;
	char* value;
	long expiry_time;
} HashEntry;


void hashtable_init(HashEntry hash_table[]);
bool hashtable_set(HashEntry hash_table[], const char* key, const char* value, long expiry_time);
char* hashtable_get(HashEntry hash_table[], char* key);
void hashtable_print(HashEntry hash_table[]);