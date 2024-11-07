#include <stdbool.h>
#include "hash.h"

#define BUFFER_SIZE 1024

int decode_string(char* dest, unsigned char* src);
int decode_size(char* dest , unsigned char* src);

bool load_from_rdb_file(HashEntry* dest_hashtable, const char* filename);
