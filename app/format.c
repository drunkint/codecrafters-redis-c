#include "format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


bool is_digit(char character) {
	return '0' <= character && character <= '9';
}

bool is_number(char* str) {
	for (int i = 0; i < strlen(str); i++) {
		if (!is_digit(str[i])) {
			return false;
		}
	}
	return true;
}

void modify_to_lower(char* str) {
	for(int i = 0; i < strlen(str); i++){
		str[i] = tolower(str[i]);
	}
}

// str has max size BUFFER_SIZE
// bulk strings are encoded as: $<length>\r\n<data>\r\n
void get_bulk_string(char* dest, char* src) {
	if (src == NULL) {
		strcpy(dest, "$-1\r\n");
		return;
	}

	int length = strlen(src);
	sprintf(dest, "$%d\r\n%s\r\n", length, src);
}

// simple strings are encoded as: +<data>\r\n
void get_simple_string(char* dest, char* src) {
	sprintf(dest, "+%s\r\n", src);
}

// RESP arrays are encoded as: *<number-of-elements>\r\n<element-1>...<element-n>
// Assumption: each element in src is encoded
// if number_of_elemnts is -1, it's a null bulck string
void get_resp_array(char* dest, char src[][MAX_ARGUMENT_LENGTH], int number_of_elements) {
	if (number_of_elements == -1) {
		strcpy(dest, "*-1\r\n");
		return;
	}
	
	if (number_of_elements == 0) {
		strcpy(dest, "*0\r\n");
		return;
	}

	char consecutive_elements[BUFFER_SIZE];
	memset(consecutive_elements, '\0', BUFFER_SIZE);
	// printf("resp start\n");
	for (int i = 0; i < number_of_elements; i++) {
		// printf("concatanating %s to %s\n", src[i], consecutive_elements);
		strcat(consecutive_elements, src[i]);
		// printf("-- resulting in %s\n", consecutive_elements);

	}
	sprintf(dest, "*%d\r\n%s", number_of_elements, consecutive_elements);
}

// RESP arrays are encoded as: *<number-of-elements>\r\n<element-1>...<element-n>
// Assumption: each element in src is encoded
void get_resp_array_pointer(char* dest, char** src, int number_of_elements) {
	if (number_of_elements == -1) {
		strcpy(dest, "*-1\r\n");
		return;
	}

	if (number_of_elements == 0) {
		strcpy(dest, "*0\r\n");
		return;
	}

	char consecutive_elements[BUFFER_SIZE];
	memset(consecutive_elements, '\0', BUFFER_SIZE);
	// printf("resp start %d\n", number_of_elements);
	for (int i = 0; i < number_of_elements; i++) {
		// printf("concatanating %s to %s\n", src[i], consecutive_elements);
		strcat(consecutive_elements, src[i]);
		// printf("-- resulting in %s\n", consecutive_elements);

	}
	sprintf(dest, "*%d\r\n%s", number_of_elements, consecutive_elements);
}


void get_simple_error(char* dest, char* prefix, char* msg) {
	sprintf(dest, "-%s %s\r\n", prefix, msg);
}

void get_integer(char* dest, long long int src) {
	sprintf(dest, ":%lld\r\n", src);
}
