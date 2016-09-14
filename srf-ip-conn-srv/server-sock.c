#include "server-sock.h"

#include "sock.h"

#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <netinet/in.h>

// As we deal with only one received packet at a time, we store it in this struct.
server_sock_received_packet_t server_sock_received_packet;
static int server_sock_fd = -1;

// Returns 1 if socket can be read. Returns -1 on error.
static int server_sock_check_read(void) {
	fd_set rfds;
	struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 }; // Blocking only for 1 second.

	if (server_sock_fd < 0)
		return -1;

	FD_ZERO(&rfds);
	FD_SET(server_sock_fd, &rfds);

	switch (select(server_sock_fd+1, &rfds, NULL, NULL, &timeout)) {
		case -1:
			syslog(LOG_ERR, "server-sock error: select() error\n");
			return -1;
		case 0: // Timeout
			return 0;
		default:
			return FD_ISSET(server_sock_fd, &rfds);
	}
}

// Receives UDP packet to server_sock_received_packet.
// Returns received number of bytes if a packet has been received, and -1 on error.
int server_sock_receive(server_sock_received_packet_t *packet) {
	socklen_t addr_len;

	switch (server_sock_check_read()) {
		case -1: return -1;
		case 0: return 0;
		default:
			addr_len = sizeof(packet->from_addr);
			if ((packet->received_bytes = recvfrom(server_sock_fd, packet->buf, sizeof(packet->buf), 0, &packet->from_addr, &addr_len)) == -1)
				return -1;

			return packet->received_bytes;
	}
}

// Sends packet given in buf with size buflen to dst_addr.
flag_t server_sock_send(uint8_t *buf, uint16_t buflen, struct sockaddr *dst_addr) {
	socklen_t addr_len = sizeof(struct sockaddr);

	return (sendto(server_sock_fd, buf, buflen, 0, dst_addr, addr_len) == buflen);
}

// Returns 1 if initialization was successful, 0 on error.
flag_t server_sock_init(uint16_t port, flag_t ipv4_only, char *bind_ip) {
	struct addrinfo hints, *servinfo, *p;
	int optval;
	char port_str[6];

	if (bind_ip[0] != 0)
		syslog(LOG_INFO, "server-sock: trying to bind to %s:%u\n", bind_ip, port);
	else
		syslog(LOG_INFO, "server-sock: trying to bind to port %u\n", port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ipv4_only ? AF_INET : AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	snprintf(port_str, sizeof(port_str), "%u", port);
	if ((server_sock_fd = getaddrinfo((bind_ip[0] != 0 ? bind_ip : NULL), port_str, &hints, &servinfo)) != 0) {
		syslog(LOG_ERR, "server-sock error: getaddrinfo error: %s\n", gai_strerror(server_sock_fd));
		server_sock_fd = -1;
		return 0;
	}

	// Loop through all the results and bind to the first we can.
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((server_sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;

		if (bind(server_sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
			close(server_sock_fd);
			continue;
		}
		break;
	}

	if (p == NULL) {
		syslog(LOG_ERR, "server-sock error: failed to bind socket to %s:%u\n", bind_ip, port);
		server_sock_fd = -1;
		return 0;
	}

	syslog(LOG_INFO, "server-sock: bound\n");

	freeaddrinfo(servinfo);

	// Setting TOS.
	optval = 184;
	setsockopt(server_sock_fd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval));

	return 1;
}

void server_sock_deinit(void) {
	if (server_sock_fd >= 0) {
		close(server_sock_fd);
		server_sock_fd = -1;
	}
}
