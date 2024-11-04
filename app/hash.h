#define HASH_NUM 1021

typedef struct HashEntry {
	char* key;
	char* value;
} HashEntry;


void hashtable_init(HashEntry hash_table[]);
bool hashtable_set(HashEntry hash_table[], const char* key, const char* value);
char* hashtable_get(HashEntry hash_table[], char* key);