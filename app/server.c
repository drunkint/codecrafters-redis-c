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
#include <limits.h>
// #include "hash.h"
#include "timer.h"
#include "rdb.h"
#include "format.h"
// #include "radix-trie.h"
#include "queue.h"

#define MAX_NUM_ARGUMENTS 8
#define MAX_NUM_FDS 10
#define INITIAL_TABLE_SIZE 4
#define pass (void)0


// system wide variables
HashTable* ht;																		// stores (key, value) and expiry date
struct pollfd fds[MAX_NUM_FDS];										// list of fds we can use.
char results[MAX_NUM_FDS][BUFFER_SIZE];						// for storing results to pass back to fds.

Queue* eq;																				// event queue: stores async events with timeout
Queue* tq;																				// trigger queue: stores async events that will be triggered on some action.
Queue* rq;																				// run queue: stores async events ready to be ran. Other queues will push their events here.

Queue* transac_q[MAX_NUM_FDS];										// transaction queues: stores transactions for each fd client.

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

// handles xadd without considering checking the trigger queue.
bool handle_xadd_without_tq(char* result, char* stream_key, char* id, char* key, char* value) {
	if (stream_key == NULL || id == NULL || key == NULL || value == NULL) {
		get_bulk_string(result, "UH-OH");
		return false;
	}

	HashEntry* entry = ht_get_entry(ht, stream_key);


	if (!check_stream_id(result, id)) {
		return false;
	}

	// explicitly use ID. No generation.
	// create stream if stream key doesn't exist
	if (entry == NULL) {
		entry = ht_set(ht, stream_key, "", TYPE_STREAM, 0);
		entry->stream = rn_create("");
	} else {
		char* latest_key = rn_get_latest_key(entry->stream);
		if (strcmp(id, latest_key) <= 0 && strstr(id, "*") == NULL) {
			get_simple_error(result, "ERR", "The ID specified in XADD is equal or smaller than the target stream top item");
			free(latest_key);
			return false;
		}
		
		free(latest_key);
	}

	if (strstr(id, "*") == NULL) {
		rn_insert(entry->stream, id, key, value);
		get_bulk_string(result, id);
		return true;
	}

	// partially generate ID when needed
	if (strstr(id, "-*") != NULL) {
		id = rn_partially_generate_key(entry->stream, id);
		rn_insert(entry->stream, id, key, value);
		get_bulk_string(result, id);
		free(id);
		return true;
	}

	id = rn_generate_key(entry->stream);
	rn_insert(entry->stream, id, key, value);
	get_bulk_string(result, id);
	free(id);
	return true;

}

void check_trigger_queue(char* keyword) {
	// When event should be ran, we run it and make it dissapear from the tq.
	Event* cur = NULL;

	while ((cur = q_find_and_pop(tq, keyword)) != NULL) {
		q_prepend(rq, cur);
	}
}

bool handle_xadd(char* result, char* stream_key, char* id, char* key, char* value) {

	if (handle_xadd_without_tq(result, stream_key, id, key, value)) {
		check_trigger_queue(stream_key);
	}
	
}


// returns false if it's empty
// gets stream entries that are greater than start_id (exclusive)
// does not wrap the single stream with a list of length 1.
// assumes category is stream
bool handle_single_xread_no_wrap(char* result, char* stream_key, char* id_start) {
	char id_end[ID_LENGTH] = {0};
	sprintf(id_end, "%lu-%lu", ULONG_MAX, ULONG_MAX);
	// printf("max id is %s\n", id_end);
	HashEntry* entry = ht_get_entry(ht, stream_key);

	if (entry == NULL) {
		get_resp_array(result, NULL, 0);
		return false;
	}

	char id_start_inclusive[ID_LENGTH] = {0};
	increment_seq_part(id_start_inclusive, id_start);
	// printf("new id start is %s\n", id_start_inclusive);

	RadixNode* acc_rn[MAX_RADIX_NODES] = {0};
	char* acc_id[MAX_RADIX_NODES] = {0};
	printf("! traversing %s [%s, %s]", entry->key, id_start_inclusive, id_end);
	int num_rn = rn_traverse(entry->stream, id_start_inclusive, id_end, acc_rn, acc_id);
	printf("resulting in %d items found\n", num_rn);
	printf("printing radix tree");
	rn_print(entry->stream->children[0]);

	if (num_rn == 0) {
		get_resp_array(result, NULL, 0);
		return false;
	}

	char id_and_data_resp[BUFFER_SIZE] = {0};
	format_radix(acc_rn, acc_id, num_rn, id_and_data_resp);

	char stream_resp[BUFFER_SIZE] = {0};
	char* stream_arr[2] = {0};
	stream_arr[0] = calloc(strlen(stream_key) + 10, sizeof(char));
	get_bulk_string(stream_arr[0], stream_key);
	stream_arr[1] = id_and_data_resp;
	get_resp_array_pointer(result, stream_arr, 2);

	free(stream_arr[0]);
	return true;
}

bool handle_xread(char* result, char args[][MAX_ARGUMENT_LENGTH] ) {
	char result_arr[MAX_NUM_ARGUMENTS / 2 + 1][MAX_ARGUMENT_LENGTH] = {0};
	char* catagory = args[0];
	char* stream_key[MAX_NUM_ARGUMENTS / 2 + 1] = {0};
	char* id_start[MAX_NUM_ARGUMENTS / 2 + 1] = {0};
	int arg_num = 0;
	while(strlen(args[arg_num]) > 0) {
		arg_num++;
	}

	int stream_num = arg_num / 2;

	for (int i = 0; i < stream_num; i++) {
		stream_key[i] = args[i + 1];
		id_start[i] = args[stream_num + i + 1];
		// printf("->%d stream, id_start: %s, %s\n", stream_num, stream_key[i], id_start[i]);
	}

	if (strcmp(catagory, "streams") != 0) {
		get_simple_error(result, "ERR", "XREAD does not accpet this type yet.");
		return false;
	}

	bool is_result_all_empty = true;
	for (int i = 0; i < stream_num; i++) {
		is_result_all_empty &= !handle_single_xread_no_wrap(result_arr[i], stream_key[i], id_start[i]);
	}

	if (is_result_all_empty) {
		get_resp_array(result, NULL, -1);
	} else {
		get_resp_array(result, result_arr, stream_num);
	}
	return true;

}

bool handle_print(char* result) {
	ht_print(ht);
	get_simple_string(result, "OK");
}

bool handle_xrange(char* result, char* stream_key, char* id_start, char* id_end) {
	// preprocess id_start
	if (strlen(id_start) == 1 && id_start[0] == '-') { // using - to indicate start
		strcpy(id_start, "0-0");
	} else if (strchr(id_start, '-') == NULL) {				// init id_start's seq num if not provided.
		char temp[MAX_ARGUMENT_LENGTH] = {0};
		strcpy(temp, id_start);
		sprintf(id_start, "%s-%lu", temp, 0);
	}

	// preprocess id_end
	if (strlen(id_end) == 1 && id_end[0] == '+') {	// using + to indicate end
		sprintf(id_end, "%lu-%lu", ULONG_MAX, ULONG_MAX);
	} else if (strchr(id_end, '-') == NULL) {				// if sequence num (as in <time>-<seq>) is not provided, id_end's seq num is set to its max
		char temp[MAX_ARGUMENT_LENGTH] = {0};
		strcpy(temp, id_end);
		sprintf(id_end, "%s-%lu", temp, ULONG_MAX);
	}

	HashEntry* entry = ht_get_entry(ht, stream_key);
	if (entry == NULL) {
		get_resp_array(result, NULL, 0);
		return true;
	}

	RadixNode* acc_rn[MAX_RADIX_NODES] = {0};
	char* acc_id[MAX_RADIX_NODES] = {0};
	int num_rn = rn_traverse(entry->stream, id_start, id_end, acc_rn, acc_id);
	format_radix(acc_rn, acc_id, num_rn, result);
	return true;
	
}

bool handle_incr(char* result, char* key) {
	char* cur_val = ht_get_value(ht, key);
	if (cur_val != NULL && !is_number(cur_val)) {
		get_simple_error(result, "ERR", "value is not an integer or out of range");
		return false;
	}

	long long val = cur_val == NULL ? 1 : atoll(cur_val) + 1;
	char val_str[256] = {0};
	sprintf(val_str, "%lld", val);
	ht_set(ht, key, val_str, TYPE_STRING, 0);

	get_integer(result, val);
	return true;
}

bool handle_multi(char* result) {

	get_simple_string(result, "OK");
}

// command is a RESP array of bulk strings
// RESP array are encoded as: *<number-of-elements>\r\n<element-1>...<element-n>
// bulk strings are encoded as: $<length>\r\n<data>\r\n
int parse_command_from_client(char decoded_command[][MAX_ARGUMENT_LENGTH], char* command) {
	if (!(strlen(command) >= 2 && command[0] == '*' && is_digit(command[1]))) {
		printf("what is this: %c\n", command[0]);
		printf("invalid RESP array: %s\n", command);
		return 1;
	}
	command += 1; // skip the *
	int resp_array_length = atoi(command);

	// decoded_command[0] is the command name. The rest are its arguments.d
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

	printf("\n (received at %lu)\n", get_time_in_ms());

}

int handle_command(char* result, char decoded_command[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH]) {
		// printf("-> handling %s at %lu\n", decoded_command[0], get_time_in_ms());

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
	} else if (strcmp(decoded_command[0], "print") == 0) {
		handle_print(result);
		return 0;
	} else if (strcmp(decoded_command[0], "xrange") == 0) {
		handle_xrange(result, decoded_command[1], decoded_command[2], decoded_command[3]);
		return 0;
	} else if (strcmp(decoded_command[0], "xread") == 0) {
		handle_xread(result, &decoded_command[1]);		// skipps "xread" itself
		return 0;
	} else if (strcmp(decoded_command[0], "incr") == 0) {
		handle_incr(result, decoded_command[1]);		
		return 0;
	} else if (strcmp(decoded_command[0], "multi") == 0) {
		handle_multi(result);
		return 0;
	} else {
		strcpy(result, "+NotImplemented\r\n");
		return 0;
	}
}

bool is_command_blocking(char decoded_command[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH]) {
	for (int i = 0; i < MAX_NUM_ARGUMENTS; i++) {
		if (strlen(decoded_command[i]) > 0 && strcmp(decoded_command[i], "block") == 0) {
			return true;
		}
	}
	return false;
}

// only considers the dollar sign placeholder as in xread block 0 streams some_key $
// in this case, replace $ with the latest key
void preprocess_blocking_command(char decoded_command[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH]) {
	if (strcmp(decoded_command[0], "xread") == 0 && strlen(decoded_command[5]) == 1 && decoded_command[5][0] == '$') {
		HashEntry* he = ht_get_entry(ht, decoded_command[4]);
		if (he == NULL) {
			strcpy(decoded_command[5], "0-0");
			return;
		}

		char* latest_key = rn_get_latest_key(he->stream);
		if (latest_key == NULL || strlen(latest_key) == 0) {
			strcpy(decoded_command[5], "0-0");
			latest_key != NULL ? free(latest_key) : pass;
			return;
		}

		strcpy(decoded_command[5], latest_key);
		free(latest_key);
		return;
	}
}

void handle_blocking_command(char decoded_command[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH], int fd_index) {
	unsigned long expiry_interval = atoll(decoded_command[2]);
	unsigned long current_time = get_time_in_ms();
	// preprocessing
	preprocess_blocking_command(decoded_command);

	// blocks until some other entry is added in this stream using XADD.
	// decoded_command example: xread block 0 streams some_key 1526985054069-0
	if (strcmp(decoded_command[0], "xread") == 0 && strcmp(decoded_command[1], "block") == 0 && expiry_interval == 0) {
		strcpy(decoded_command[2], "xread");

		// push to trigger queue
		q_add(tq, &decoded_command[2], 4, 0, fd_index);
		return;
	}


	// get the original command without block and the expiry time.
	// decoded_command example: xread block 1000 streams some_key 1526985054069-0
	if (strcmp(decoded_command[0], "xread") == 0 && strcmp(decoded_command[1], "block") == 0 && expiry_interval > 0) {
		unsigned long expiry_time = expiry_interval + current_time;
		strcpy(decoded_command[2], "xread");

		// push to event queue.
		q_add(eq, &decoded_command[2], 4, expiry_time, fd_index);
		return;
	}

	printf("Error! Blocking (cmd: '%s') for the command '%s' is not supported.\n", decoded_command[1], decoded_command[0]);
}

// moves ready events to the ready queue.
void check_event_queue() {
	// printf("-> checking queue at %lu\n", get_time_in_ms());
	while (q_is_head_expired(eq)) {
		
		Event* e = q_pop_front(eq);
		if (e == NULL) {
			break;
		}

		// ready queue will be ran asyncly
		q_prepend(rq, e);
	}
}

void run_all_in_queue(Queue* q) {
	Event* e;
	while (e = q_pop_front(q)) {
		if (e == NULL) {
			break;
		}

		// bad implementation but put here for now.
		char temp[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH] = {0};
		for (int i = 0; i < e->command_length; i++) {
			strcpy(temp[i], e->command[i]);
			printf("copied over: %s\n", temp[i]);
		}
		// printf("-> handling %s at %lu\n", temp[0], get_time_in_ms());
		handle_command(results[e->fd_index], temp);
		fds[e->fd_index].events |= POLLOUT; 
		
		q_destroy_event(e);
	}
}

int main(int argc, char *argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// create event queue
	eq = q_init();
	tq = q_init();
	rq = q_init();
	for (int i = 0; i < MAX_NUM_FDS; i++) {
		transac_q[i] = q_init();
	}

	// create database (hashtable)
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
		int num_of_fds_with_event = poll(fds, MAX_NUM_FDS, 100);
		// printf("num_of_fds_with_event: %d\n", num_of_fds_with_event);

		if (num_of_fds_with_event < 0) {
			printf("Poll failed: %s \n", strerror(errno));
			return 1;
		}

		check_event_queue();
		run_all_in_queue(rq);
		if (num_of_fds_with_event == 0) {
			continue;
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
				char buffer[BUFFER_SIZE];
				// char result[BUFFER_SIZE];
				memset(buffer, '\0', BUFFER_SIZE);
				// memset(result, '\0', BUFFER_SIZE);				

				// this is not blocking because fds[i].revents includes POLLIN in poll()
				// this means there are data to read in the client fd
				ssize_t read_length = read(fds[i].fd, buffer , BUFFER_SIZE);
				
				if (read_length <= 0) {
					printf("Client disconnected\n");
					close(fds[i].fd);
					fds[i].fd = -1; 
					memset(results[i], '\0', BUFFER_SIZE);
				} else {
					char decoded_command[MAX_NUM_ARGUMENTS][MAX_ARGUMENT_LENGTH] = {0};
					parse_command_from_client(decoded_command, buffer);

					// printf("hey\n");
					if (is_command_blocking(decoded_command)) {
						handle_blocking_command(decoded_command, i);
						continue;
					}

					handle_command(results[i], decoded_command);
					// strcpy(results[i], result);
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

