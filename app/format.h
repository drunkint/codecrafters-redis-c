#include <stdbool.h>

#define BUFFER_SIZE 1024
#define MAX_ARGUMENT_LENGTH 256


bool is_digit(char character);
void modify_to_lower(char* str);
void get_bulk_string(char* dest, char* src);
void get_simple_string(char* dest, char* src);
void get_resp_array(char* dest, char src[][MAX_ARGUMENT_LENGTH], int number_of_elements);