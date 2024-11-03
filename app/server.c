#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void* handle_client(void* client_fd_pointer) {
	int client_fd = (int)(*(int*)client_fd_pointer);
	char buffer[BUFFER_SIZE];
	memset(buffer, '\0', BUFFER_SIZE);

	while(read(client_fd, buffer , BUFFER_SIZE) > 0) {
		write(client_fd, "+PONG\r\n", strlen("+PONG\r\n"));
		memset(buffer, '\0', BUFFER_SIZE);
	}

	printf("Client disconnected\n");
	close(client_fd);  // Close the client connection
	return NULL;
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
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	while (1) {
		int *client_fd = malloc(sizeof(int)); // Dynamic allocation for the client file descriptor
		*client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		printf("Client connected\n");

		pthread_t t;
		int thread_result = pthread_create(&t, NULL, handle_client, client_fd) != 0;
		if (thread_result != 0) {
			fprintf(stderr, "Failed to create thread: %s\n", strerror(thread_result));
			close(*client_fd);
			free(client_fd);
			continue;
		}

		pthread_detach(t);
	}
	
	

	close(server_fd);

	return 0;
}
