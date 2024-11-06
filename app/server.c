#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <poll.h>
#include "hash.h"
#include "timer.h"

#define BUFFER_SIZE 1024
#define MAX_ARGUMENT_LENGTH 256
#define MAX_NUM_ARGUMENTS 8
#define MAX_NUM_FDS 10

HashEntry hashtable[HASH_NUM];
struct pollfd fds[MAX_NUM_FDS];


bool add_to_fds(int fd) {
	for (int i = 1; i < MAX_NUM_FDS; i++) {
		if (fds[i].fd == -1) {
			fds[i].fd = fd;
			fds[i].events = POLLIN;
			return true;
		}
	}
	return false;
}

bool is_digit(char character) {
	return '0' <= character && character <= '9';
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

bool handle_set(char* result, char* key, char* value, char* flag, char* arg) {
	long expiry_time = -1;
	if (strcmp(flag, "px") == 0) {
		expiry_time = (long)atoi(arg) + get_time_in_ms();
	}

	printf("in handle set, expiry_time is: %d\n", expiry_time);

	if (!hashtable_set(hashtable, key, value, expiry_time)) {
		get_simple_string(result, "ERROR-SET");
		return false;
	}

	hashtable_print(hashtable);

	get_simple_string(result, "OK");
	return true;
}

bool handle_get(char* result, char* key) {
	char* value = hashtable_get(hashtable, key);
	get_bulk_string(result, value);
	return true;
}

// command is a RESP array of bulk strings
// RESP array are encoded as: *<number-of-elements>\r\n<element-1>...<element-n>
// bulk strings are encoded as: $<length>\r\n<data>\r\n
int parse_command_from_client(char* result, char* command) {
	if (!(strlen(command) >= 2 && command[0] == '*' && is_digit(command[1]))) {
		printf("what is this: %c\n", command[0]);
		printf("invalid RESP array: %s\n", command);
		return 1;
	}
	command += 1; // skip the *
	int resp_array_length = atoi(command);

	// decoded_command[0] is the command name. The rest are its arguments.d
	char decoded_command[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH];
	memset(decoded_command, '\0', MAX_NUM_ARGUMENTS * MAX_ARGUMENT_LENGTH);
	
	char* cur = strchr(command, '\n') + 1;
	for (int i = 0; i < resp_array_length; i++) {
		if (cur[0] != '$') {
			printf("invalid RESP array: %s, reason: bulk string encoding does not start with $", command, cur[0]);
			return 1;
		}
		cur++;

		if (!is_digit(cur[0])) {
			printf("invalid RESP array: %s, reason: %s is not a digit", command, cur[0]);
			return 1;
		}

		int elem_length = atoi(cur);
		cur = strchr(cur, '\n') + 1;

		strncpy(decoded_command[i], cur, elem_length);

		if (!is_digit(decoded_command[i][0])) {
			modify_to_lower(decoded_command[i]);
		}
		printf("decoded_command[%d]: %s\n", i, decoded_command[i]);

		cur = strchr(cur, '\n') + 1; // skip <data>\r\n
	}

	// printf("decoded_command[0]: %s\n", decoded_command[0]);
	if (strcmp(decoded_command[0], "ping") == 0) {
		get_simple_string(result, "PONG");
		return 0;
	} else if (strcmp(decoded_command[0], "echo") == 0) {
		get_bulk_string(result, decoded_command[1]);
		return 0;
	} else if (strcmp(decoded_command[0], "set") == 0) {
		handle_set(result, decoded_command[1], decoded_command[2], decoded_command[3], decoded_command[4]);
		return 0;
	} else if (strcmp(decoded_command[0], "get") == 0) {
		handle_get(result, decoded_command[1]);
		return 0;
	} else {
		strcpy(result, "+NotImplemented\r\n");
		return 0;
	}

}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(6379),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	hashtable_init(hashtable);

	// start of event loop
	printf("Waiting for clients to connect...\n");
	
	// init first fd to be the server fd
	fds[0].fd = server_fd;
	fds[0].events = POLLIN;

	// init the remaining fds (these will be filled with client fds later) 
	for (int i = 1; i < MAX_NUM_FDS; i++) {
		fds[i].fd = -1;
	}

	while(1) {
		// poll() is the only blocking line of code 
		int num_of_fds_with_event = poll(fds, MAX_NUM_FDS, -1);
		// printf("num_of_fds_with_event: %d\n", num_of_fds_with_event);

		if (num_of_fds_with_event < 0) {
			printf("Poll failed: %s \n", strerror(errno));
			return 1;
		}

		// checks if there's a new connection request
		if (fds[0].revents & POLLIN) {
			// printf("New Connection request!\n");
			
			// this line doesn't block because there are incoming data
			int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
			printf("Accepted connection request from fd: %d\n", client_fd);

			if (!add_to_fds(client_fd)) {
				printf("Too many fds: %s \n", strerror(errno));
				return 1;
			}
		}

		// handle each client
		for (int i = 1; i < MAX_NUM_FDS; i++) {
			// if POLLIN is included in fds[i].revents (& is bitwise and)
			// Note: fds[i].revents is populated by poll()
			if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) { 
				char buffer[BUFFER_SIZE], result[BUFFER_SIZE];
				memset(buffer, '\0', BUFFER_SIZE);
				memset(result, '\0', BUFFER_SIZE);				

				ssize_t read_length = read(fds[i].fd, buffer , BUFFER_SIZE);
				
				if (read_length <= 0) {
					printf("Client disconnected\n");
					close(fds[i].fd);
					fds[i].fd = -1; 
				} else {
					parse_command_from_client(result, buffer);
					write(fds[i].fd, result, strlen(result));
				}
			}
		}
	}
	
	close(server_fd);

	return 0;
}

