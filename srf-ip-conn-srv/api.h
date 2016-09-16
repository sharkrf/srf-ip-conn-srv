#ifndef API_H_
#define API_H_

#include "types.h"

#include <time.h>
#include <stdlib.h>

typedef struct api_client {
	int sock;
	time_t last_valid_packet_got_at;

	struct api_client *next;
	struct api_client *prev;
} api_client_t;

int api_add_clients_to_fd_set(fd_set *fds);
void api_process_fd_set(fd_set *fds);

void api_client_add(int sock);

void api_process(void);
int api_init(char *api_socket_file);

#endif
