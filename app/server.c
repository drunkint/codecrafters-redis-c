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
// #include "hash.h"
#include "timer.h"
#include "rdb.h"
#include "format.h"
// #include "radix-trie.h"

#define MAX_NUM_ARGUMENTS 8
#define MAX_NUM_FDS 10
#define INITIAL_TABLE_SIZE 4


// system wide variables
HashTable* ht;																		// stores (key, value) and expiry date
struct pollfd fds[MAX_NUM_FDS];										// list of fds we can use.
char results[MAX_NUM_FDS][BUFFER_SIZE];						// for storing results to pass back to fds.

char dir[MAX_ARGUMENT_LENGTH] = {0};
char db_filename[MAX_ARGUMENT_LENGTH] = {0};


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

bool handle_arguments(int argc, char* argv[]) {
	for (int i = 0; i < argc; i++) {
		if (strncmp(argv[i], "--", 2) == 0) {
			char* option_name = argv[i] + 2; // skip the --

			if (strcmp(option_name, "dir") == 0) {
				i++;
				strcpy(dir, argv[i]);
			} else if (strcmp(option_name, "dbfilename") == 0) {
				i++;
				strcpy(db_filename, argv[i]);
			}
		}
		// printf("argv[%d]: %s\n", i, argv[i]);
	}
}

bool handle_set(char* result, char* key, char* value, char* flag, char* arg) {
	// preprocess expiry time
	unsigned long expiry_time = 0;
	if (strcmp(flag, "px") == 0) {
		unsigned long expiry_interval = (unsigned long)atoi(arg);
		if (expiry_interval == 0) {
			return true;
		}
		expiry_time = expiry_interval + get_time_in_ms();
	}
	// printf("in handle set, expiry_time is: %d\n", expiry_time);


	ht_set(ht, key, value, TYPE_STRING, expiry_time);
	ht = ht_handle_resizing(ht);


	get_simple_string(result, "OK");
	return true;
}

bool handle_get(char* result, char* key) {
	char* value = ht_get_value(ht, key);
	get_bulk_string(result, value);
	return true;
}

bool handle_delete(char* result, char* key) {
	ht_delete(ht, key);
	get_simple_string(result, "OK");
	return true;
}

bool handle_config_get(char* result, char* raw_name) {
	char name[MAX_ARGUMENT_LENGTH];
	char value[MAX_ARGUMENT_LENGTH];
	get_bulk_string(name, raw_name);
	if (strcmp(raw_name, "dir") == 0) {
		get_bulk_string(value, dir);
	} else if (strcmp(raw_name, "dbfilename") == 0) {
		get_bulk_string(value, db_filename);
	}

	char raw_result[2][MAX_ARGUMENT_LENGTH];
	strcpy(raw_result[0], name);
	strcpy(raw_result[1], value);
	get_resp_array(result, raw_result, 2);
	return true;
}

void freeStrings(char** src, int length) {
	for (int i = 0; i < length; i++) {
		free(src[i]);
	}
	free(src);
}	

bool handle_keys(char* result, char* pattern) {
	// check if pattern is *. Only supports * at this time.
	if (pattern[0] == '*') {
		char** raw_result = ht_get_keys(ht, NULL);
		char** result_before_formatting = calloc(ht->num_of_elements, sizeof(char*));

		for (int i = 0; i < ht->num_of_elements; i++) {
			char temp[MAX_ARGUMENT_LENGTH] = {0};
			printf("key[%d]: %s\n", i, raw_result[i]);
			get_bulk_string(temp, raw_result[i]);

			result_before_formatting[i] = calloc(strlen(temp) + 1, sizeof(char));
			strcpy(result_before_formatting[i], temp);
		}
		get_resp_array_pointer(result, result_before_formatting, ht->num_of_elements);

		freeStrings(raw_result, ht->num_of_elements);
		freeStrings(result_before_formatting, ht->num_of_elements);

		return true;
	}

	return false;
}

bool handle_type(char* result, char* key) {
	char raw_result[BUFFER_SIZE];

	Type type = ht_get_type(ht, key);
	get_type_string(raw_result, type);

	get_simple_string(result, raw_result);
	return true;
}

bool handle_xadd(char* result, char* stream_key, char* id, char* key, char* value) {
	if (stream_key == NULL || id == NULL || key == NULL || value == NULL) {
		get_bulk_string(result, "UH-OH");
		return false;
	}

	HashEntry* entry = ht_get_entry(ht, stream_key);
	if (entry == NULL) {
		entry = ht_set(ht, stream_key, "", TYPE_STREAM, 0);
		entry->stream = rn_create("");
	}
	rn_insert(entry->stream, id, key, value);

	get_bulk_string(result, id);
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
	
	printf("decoded command: ");

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
		printf("%s ", decoded_command[i]);

		cur = strchr(cur, '\n') + 1; // skip <data>\r\n
	}

	printf("\n");
	

	// printf("decoded_command[0]: %s\n", decoded_command[0]);
	if (strcmp(decoded_command[0], "ping") == 0) {
		get_simple_string(result, "PONG");
		return 0;
	} else if (strcmp(decoded_command[0], "echo") == 0) {
		get_bulk_string(result, decoded_command[1]);
		return 0;
	} else if (strcmp(decoded_command[0], "set") == 0) {
		handle_set(result, decoded_command[1], decoded_command[2], decoded_command[3], decoded_command[4]);
		ht_print(ht);
		return 0;
	} else if (strcmp(decoded_command[0], "get") == 0) {
		handle_get(result, decoded_command[1]);
		ht_print(ht);
		return 0;
	} else if (strcmp(decoded_command[0], "delete") == 0) {
		handle_delete(result, decoded_command[1]);
		ht_print(ht);
		return 0;
	} else if (strcmp(decoded_command[0], "config") == 0 && strcmp(decoded_command[1], "get") == 0) {
		handle_config_get(result, decoded_command[2]);
		return 0;
	} else if (strcmp(decoded_command[0], "keys") == 0) {
		handle_keys(result, decoded_command[1]);
		return 0;
	} else if (strcmp(decoded_command[0], "type") == 0) {
		handle_type(result, decoded_command[1]);
		return 0;
	} else if (strcmp(decoded_command[0], "xadd") == 0) {
		handle_xadd(result, decoded_command[1], decoded_command[2], decoded_command[3], decoded_command[4]);
		return 0;
	} else {
		strcpy(result, "+NotImplemented\r\n");
		return 0;
	}

}

int main(int argc, char *argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");


	ht = ht_create_table(INITIAL_TABLE_SIZE);
	handle_arguments(argc, argv);
	if (strlen(dir) > 0 || strlen(db_filename) > 0) {
		char db_complete_filename[MAX_ARGUMENT_LENGTH];
		sprintf(db_complete_filename, "%s/%s", dir, db_filename);

		load_from_rdb_file(ht, db_complete_filename);
		ht_print(ht);
	}
	
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
			// printf("Accepted connection request from fd: %d\n", client_fd);

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

				// this is not blocking because fds[i].revents includes POLLIN in poll()
				// this means there are data to read in the client fd
				ssize_t read_length = read(fds[i].fd, buffer , BUFFER_SIZE);
				
				if (read_length <= 0) {
					printf("Client disconnected\n");
					close(fds[i].fd);
					fds[i].fd = -1; 
					memset(results[i], '\0', BUFFER_SIZE);
				} else {
					parse_command_from_client(result, buffer);
					strcpy(results[i], result);
					fds[i].events |= POLLOUT; 
				}
			}

			// if POLLOUT is included in fds[i].revents (& is bitwise and)
			// this is not blocking because fds[i].revents include POLLOUT in poll()
			if (fds[i].fd != -1 && (fds[i].revents & POLLOUT) && strlen(results[i]) > 0) {
				write(fds[i].fd, results[i], strlen(results[i]));
				memset(results[i], '\0', BUFFER_SIZE);
			}
		}

	}
	
	close(server_fd);

	return 0;
}

