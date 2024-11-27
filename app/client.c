#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <poll.h>
#include <limits.h>
#include "timer.h"
#include "format.h"
#include "client.h"

int send_cmd(struct pollfd* fd, const char* coded_cmd) {
  if (fd->fd == -1) {
    return 0;
  }
  
  fd->events = POLLOUT;
  ssize_t bytes_sent = write(fd->fd, coded_cmd, strlen(coded_cmd));
  if (bytes_sent < 0) {
      printf("Failed to send cmd to master: %s\n", strerror(errno));
      close(fd->fd);
      fd->fd = -1;
  } else {
      printf("sent to master\n", coded_cmd);
      fd->events = POLLIN; // Now wait for a response
      char buffer[BUFFER_SIZE] = {0};
      read(fd->fd, buffer, BUFFER_SIZE);
      printf("received: %s\n", buffer);
  }
}

int send_ping(struct pollfd* fd) {
  const char* ping = "*1\r\n$4\r\nPING\r\n";
  // printf("start to send ping %d %d\n", fd->fd, fd->revents);
  
  return send_cmd(fd, ping);
}

void send_replconf_step(struct pollfd* fd, int slave_port) {
  char cmd_port[MAX_ARGUMENT_LENGTH] = {0};
  sprintf(cmd_port, "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n%d\r\n", slave_port);
  send_cmd(fd, cmd_port);

  const char* cmd_capa = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
  send_cmd(fd, cmd_capa);

  return;
}

// on success return master fd, 1 o/w
int connect_to_master(char* master_host, int master_port) {
  int master_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (master_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
	}

  struct sockaddr_in master_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(master_port),
  };
  if (inet_pton(AF_INET, strcmp(master_host, "localhost") == 0 ? "127.0.0.1" : master_host, &master_addr.sin_addr) <= 0) {
      printf("Invalid master address: %s\n", master_host);
      return 1;
  }

  if (connect(master_fd, (struct sockaddr*)&master_addr, sizeof(master_addr)) < 0) {
      if (errno != EINPROGRESS) {
          printf("Failed to connect to master: %s\n", strerror(errno));
          close(master_fd);
          return 1;
      }
  }

  return master_fd;
	
}