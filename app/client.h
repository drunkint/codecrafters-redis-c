
int connect_to_master(char* master_host, int master_port);
int send_ping(struct pollfd* fd);
void send_replconf_step(struct pollfd* fd, int slave_port);
int send_psync_step(struct pollfd* fd);