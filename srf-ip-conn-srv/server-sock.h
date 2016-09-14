#ifndef SERVER_SOCK_H_
#define SERVER_SOCK_H_

#include "types.h"

#include <srf-ip-conn/common/srf-ip-conn-packet.h>

#include <sock.h>

typedef struct {
	uint8_t buf[sizeof(srf_ip_conn_packet_t)];
	uint16_t received_bytes;
	struct sockaddr from_addr;
} server_sock_received_packet_t;

int server_sock_receive(server_sock_received_packet_t *packet);
flag_t server_sock_send(uint8_t *buf, uint16_t buflen, struct sockaddr *dst_addr);

flag_t server_sock_init(uint16_t port, flag_t ipv4_only, char *bind_ip);
void server_sock_deinit(void);

#endif
