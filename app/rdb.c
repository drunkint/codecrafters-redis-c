#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rdb.h"
#include "timer.h"
/*
  Docs: https://rdb.fnordig.de/file_format.html#length-encoding
  CodeCrafters: https://app.codecrafters.io/courses/redis/stages/jz6?repo=13fd2cd4-3c72-462d-bb2f-d2dd56ab8049

*/

int size_of_key_value_ht;
int size_of_expiry_ht;

/*
  returns number of unsigned chars used (skipped)

  The 0x0D size specifies that the string is 13 characters unsigned long.
  The remaining characters spell out "Hello, World!".
    0D 48 65 6C 6C 6F 2C 20 57 6F 72 6C 64 21

  !! For sizes that begin with 0b11, the remaining 6 bits indicate a type of string format:

  The 0xC0 size indicates the string is an 8-bit integer.
  In this example, the string is "123". 
    C0 7B

  The 0xC1 size indicates the string is a 16-bit integer.
  The remaining bytes are in little-endian (read right-to-left).
  In this example, the string is "12345". 
  C1 39 30

  The 0xC2 size indicates the string is a 32-bit integer.
  The remaining bytes are in little-endian (read right-to-left),
  In this example, the string is "1234567".
  C2 87 D6 12 00

  The 0xC3 size is not supported. It's compressed with the LZF algorithm.
*/
int decode_string(char* dest, unsigned char* src) {
  if (src[0] <= 0x0D) {
    int length = (int) src[0];
    strncpy(dest, (char *)(src + 1), length); // skip the length number at the front
    return length + 1;
  } else if (src[0] == 0xC0) {
    sprintf(dest, "%d", src[1]);
    return 2;
  } else if (src[0] == 0xC1) {
    sprintf(dest, "%d", (int)(src[2] << 8) + (int)(src[1]));
    return 3;
  } else if (src[0] == 0xC2) {
    sprintf(dest, "%d", (int) (src[4] << 24) + (int) (src[3] << 16) + (int) (src[2] << 8) + (int) src[1]);
    return 5;
  } 

  return 0;

}


/*  
  If the first two bits are 0b00: <- 0b means binary, just like 0x means hexidecimal
  The size is the remaining 6 bits of the byte.
  In this example, the size is 10: 
    0A                  <- hex
    00001010            <- binary

  If the first two bits are 0b01:
  The size is the next 14 bits
  (remaining 6 bits in the first byte, combined with the next byte)
  In this example, the size is 700: 
    42 BC
    01000010 10111100

  If the first two bits are 0b10:
  Ignore the remaining 6 bits of the first byte.
  The size is the next 4 bytes, in big-endian (read left-to-right).
  In this example, the size is 17000: 
    80 00 00 42 68
    10000000 00000000 00000000 01000010 01101000

  If the first two bits are 0b11:
  The remaining 6 bits specify a type of string encoding.
  See string encoding section. 
*/
int decode_size(char* dest , unsigned char* src) {
  unsigned char first_two_bits = src[0] >> 6;
  // printf("first two bits: %u\n", first_two_bits);

  int num_of_bytes_used = 0;
  int result = 0;
  if (first_two_bits == 0b00) {
    result = (int) src[0];
    num_of_bytes_used = 1;
  } else if (first_two_bits == 0b01) {
    result = (int) (src[0] & 0x3F); // take the last 6 bits of the first byte, cast to int to prevent overflow
    result = (result << 8) + (int) src[1]; // use the 6 bits of the first byte and the 8 bits of the second
    num_of_bytes_used = 2;
  } else if (first_two_bits == 0b10) {
    result = (int) (src[1] << 24) + (int) (src[2] << 16) + (int) (src[3] << 8) + (int) src[4];
    num_of_bytes_used = 3;
  } else if (first_two_bits == 0b11) {
    num_of_bytes_used = decode_string(dest, src);
  }
  sprintf(dest, "%d", result);
  return num_of_bytes_used;
}

unsigned long decode_timestamp_milliseconds(unsigned char* src) {
  printf("printing each individually: ");
  for (int i = 0; i < 8; i++) {
    printf("%u", src[i]);
  }
  printf("\n");
  unsigned long result = ((unsigned long) src[0] << 56)
                        + ((unsigned long) src[1] << 48)
                        + ((unsigned long) src[2] << 40)
                        + ((unsigned long) src[3] << 32)
                        + ((unsigned long) src[4] << 24)
                        + ((unsigned long) src[5] << 16)
                        + ((unsigned long) src[6] << 8)
                        + ((unsigned long) src[7]);
  printf("expire argument in milliseconds: %lu\n", result);
  
  return result + get_time_in_ms();
}

unsigned long decode_timestamp_seconds(unsigned char* src) {
  unsigned long result = (unsigned long) src[0] << 24
                        + (unsigned long) src[1] << 16
                        + (unsigned long) src[2] << 8
                        + (unsigned long) src[3];
  printf("expire argument in seconds: %lu\n", result);

  return result * 1000 + get_time_in_ms();
}

bool load_from_rdb_file(HashEntry* dest_hashtable, const char* filename) {
  FILE* f;
  f = fopen(filename, "rb");
  if (f == NULL) {
    return false;
  }

  unsigned char content[BUFFER_SIZE];
  if (fread(content, sizeof(char), BUFFER_SIZE, f) < 0) {
    return false;
  }
  // printf("\n\nread file: %s\n\n", content);

  // start parsing 
  int index = 0;

  // skip header (Magic string + version number (ASCII): "REDIS0011")
  index += 9;

  printf("first elem is %d\n", content[0]);

  // Metadata section
  while (content[index] == 0xFA) {
    index++;
    char key[100] = {0};
    char value[100] = {0};
    int key_length = decode_string(key, &content[index]);
    if (key_length == 0) {
      printf("Error: incorrect string encoding of key: %x\n", content[index]);
      break;
    }
    index += key_length;

    int value_length = decode_string(value, &content[index]);
    if (value_length == 0) {
      printf("Error: incorrect string encoding of value: %x\n", content[index]);
      break;
    }
    index += value_length;
    printf("index end: (key, value) = %d: (%s, %s)\n", index, key, value);
  }

  // index of database. Notice we only support 1 at this moment.
  if (content[index] == 0xFE) {
    index++;
    char result[100];
    int index_of_database_length = decode_size(result, &content[index]);
    printf("index of database: %s, length is: %d\n", result, index_of_database_length);
    index += index_of_database_length;
  }

  // hash table information.
  if (content[index] == 0xFB) {\
    index++;

    char size_of_key_value_ht_string[100];
    int size_of_key_value_ht_string_length = decode_size(size_of_key_value_ht_string, &content[index]);
    size_of_key_value_ht = atoi(size_of_key_value_ht_string);
    index += size_of_key_value_ht_string_length;

    char size_of_expiry_ht_string[100];
    int size_of_key_expiry_string_length = decode_size(size_of_expiry_ht_string, &content[index]);
    size_of_expiry_ht = atoi(size_of_expiry_ht_string);
    index += size_of_key_expiry_string_length;
  }

  // database content (ignore expiry dates for now)
  while(content[index] != 0xFF) {
    unsigned long expiry_time = 0;
    switch (content[index])
    {
    case 0xFC:
      index++;
      expiry_time = decode_timestamp_milliseconds(&content[index]);
      index += 8; // skip for now
      break;
    case 0xFD:
      index++;
      expiry_time = decode_timestamp_seconds(&content[index]);
      index += 4; // skip for now
      break;
    default:
      break;
    }

    // only support value type = string for now
    if (content[index] == 0x00) {
      index++;
      char key[100] = {0};
      int key_length = decode_string(key, &content[index]);
      index += key_length;

      char value[100] = {0};
      int value_length = decode_string(value, &content[index]);
      index += value_length;

      printf("index end: (key, value, expiry, cur time) = %d: (%s, %s, %lu, %lu)\n", index, key, value, expiry_time, get_time_in_ms());

      hashtable_set(dest_hashtable, key, value, expiry_time);
      }
  }
  

  
}