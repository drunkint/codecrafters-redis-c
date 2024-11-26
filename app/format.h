#include <stdbool.h>

#define BUFFER_SIZE 1024
#define MAX_ARGUMENT_LENGTH 128


bool is_digit(char character);
bool is_number(char* str);
void modify_to_lower(char* str);
void get_bulk_string(char* dest, char* src);
void get_simple_string(char* dest, char* src);
void get_resp_array(char* dest, char src[][MAX_ARGUMENT_LENGTH], int number_of_elements);
void get_resp_array_pointer(char* dest, char** src, int number_of_elements);
void get_simple_error(char* dest, char* prefix, char* msg);
void get_integer(char* dest, long long int src);