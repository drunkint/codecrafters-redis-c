#include <stdbool.h>
#include "hash.h"
int decode_string(char* dest, unsigned char* src);
int decode_size(char* dest , unsigned char* src);

bool load_from_rdb_file(HashTable* dest_hashtable, const char* filename);