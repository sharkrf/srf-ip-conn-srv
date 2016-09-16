/*

Copyright (c) 2016 SharkRF OÜ. https://www.sharkrf.com/
Author: Norbert "Nonoo" Varga, HA2NON

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

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

// Sends packet given in buf with size buflen to dst_addr.
flag_t server_sock_send(uint8_t *buf, uint16_t buflen, struct sockaddr *dst_addr) {
	socklen_t addr_len = sizeof(struct sockaddr);

	return (sendto(server_sock_fd, buf, buflen, 0, dst_addr, addr_len) == buflen);
}

// Returns server_sock_fd if initialization was successful, -1 on error.
int server_sock_init(uint16_t port, flag_t ipv4_only, char *bind_ip) {
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
		return server_sock_fd;
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
		return server_sock_fd;
	}

	syslog(LOG_INFO, "server-sock: bound\n");

	freeaddrinfo(servinfo);

	// Setting TOS.
	optval = 184;
	setsockopt(server_sock_fd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval));

	return server_sock_fd;
}
