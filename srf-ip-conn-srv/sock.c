#include "sock.h"

#include <string.h>

// Get sockaddr, IPv4 or IPv6.
void *sock_get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &SOCK_ADDR_IN_ADDR(sa);
	return &SOCK_ADDR_IN6_ADDR(sa);
}

// Get port, IPv4 or IPv6.
uint16_t sock_get_port(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return SOCK_ADDR_IN_PORT(sa);
	return SOCK_ADDR_IN6_PORT(sa);
}

flag_t sock_is_sockaddr_ip_match(struct sockaddr *sa1, struct sockaddr *sa2) {
	if (sa1->sa_family != sa2->sa_family)
		return 0;

	if (sa1->sa_family == AF_INET) {
		return (SOCK_ADDR_IN_ADDR(sa1).s_addr == SOCK_ADDR_IN_ADDR(sa2).s_addr);
	} else {
		return (memcmp(&SOCK_ADDR_IN6_ADDR(sa1),
				&SOCK_ADDR_IN6_ADDR(sa2),
				sizeof(SOCK_ADDR_IN6_ADDR(sa1))) == 0);
	}
}

flag_t sock_is_sockaddr_match(struct sockaddr *sa1, struct sockaddr *sa2) {
	if (!sock_is_sockaddr_ip_match(sa1, sa2))
		return 0;

	if (sa1->sa_family == AF_INET)
		return (SOCK_ADDR_IN_PORT(sa1) == SOCK_ADDR_IN_PORT(sa2));
	else
		return (SOCK_ADDR_IN6_PORT(sa1) == SOCK_ADDR_IN6_PORT(sa2));
}
