#include "client.h"
#include "config.h"
#include "server-sock.h"

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

// We store clients waiting for login here.
static client_t *clients_login = NULL;
uint16_t clients_login_count = 0;

// We store authenticated clients here.
static client_t *clients = NULL;
uint16_t clients_count = 0;

static ignored_ip_t *client_ignored_ips = NULL;

// Searches for the given ignored IP.
ignored_ip_t *client_ignored_ip_search(struct sockaddr *addr) {
	ignored_ip_t *ipp = client_ignored_ips;

	while (ipp) {
		if (sock_is_sockaddr_ip_match(&ipp->addr, addr))
			return ipp;
		ipp = ipp->next;
	}
	return NULL;
}

void client_ignored_ip_add(struct sockaddr *addr) {
	ignored_ip_t *newip;

	newip = client_ignored_ip_search(addr);
	if (newip == NULL) {
		newip = (ignored_ip_t *)calloc(sizeof(ignored_ip_t), 1);
		if (newip == NULL)
			return;
	} else {
		// If the IP is already in the list, we just refresh it's timestamp.
		newip->added_at = time(NULL);
		return;
	}

	memcpy(&newip->addr, addr, sizeof(struct sockaddr));
	newip->added_at = time(NULL);

	if (client_ignored_ips == NULL)
		client_ignored_ips = newip;
	else {
		// Inserting to the beginning of the ignored IP list.
		client_ignored_ips->prev = newip;
		newip->next = client_ignored_ips;
		client_ignored_ips = newip;
	}
}

static void client_ignored_ip_delete(ignored_ip_t *ip) {
	if (ip->next)
		ip->next->prev = ip->prev;
	if (ip->prev)
		ip->prev->next = ip->next;
	if (ip == client_ignored_ips)
		client_ignored_ips = ip->next;
	free(ip);
}

// Searches for the given connection amongst the login clients.
client_t *client_login_search(struct sockaddr *from_addr) {
	client_t *cp = clients_login;

	while (cp) {
		if (sock_is_sockaddr_match(&cp->from_addr, from_addr))
			return cp;
		cp = cp->next;
	}
	return NULL;
}

client_t *client_login_add(uint32_t client_id, struct sockaddr *from_addr) {
	client_t *newclient;
	int i;

	newclient = client_login_search(from_addr);
	if (newclient == NULL) {
		newclient = (client_t *)calloc(sizeof(client_t), 1);
		if (newclient == NULL)
			return NULL;
	}

	newclient->client_id = client_id;
	for (i = 0; i < SRF_IP_CONN_TOKEN_LENGTH; i++)
		newclient->token[i] = rand();
	memcpy(&newclient->from_addr, from_addr, sizeof(struct sockaddr));
	newclient->last_valid_packet_got_at = time(NULL);

	if (clients_login == NULL)
		clients_login = newclient;
	else {
		// Inserting to the beginning of the login clients list.
		clients_login->prev = newclient;
		newclient->next = clients_login;
		clients_login = newclient;
	}

	clients_login_count++;

	return newclient;
}

void client_login_delete(client_t *client, flag_t free_allocated_memory) {
	clients_login_count--;
	if (client->next)
		client->next->prev = client->prev;
	if (client->prev)
		client->prev->next = client->next;
	if (client == clients_login)
		clients_login = client->next;
	if (free_allocated_memory)
		free(client);
}

// Searches for the given connection amongst the clients.
client_t *client_search(struct sockaddr *from_addr) {
	client_t *cp = clients;

	while (cp) {
		if (sock_is_sockaddr_match(&cp->from_addr, from_addr))
			return cp;
		cp = cp->next;
	}
	return NULL;
}

void client_add(client_t *client) {
	if (clients == NULL)
		clients = client;
	else {
		// Inserting to the beginning of the clients list.
		clients->prev = client;
		client->next = clients;
		clients = client;
	}

	client->last_valid_packet_got_at = time(NULL);

	clients_count++;
}

void client_delete(client_t *client) {
	clients_count--;
	if (client->next)
		client->next->prev = client->prev;
	if (client->prev)
		client->prev->next = client->next;
	if (client == clients)
		clients = client->next;
	free(client);
}

void client_broadcast(client_t *from_client, srf_ip_conn_packet_t *packet, uint16_t data_len, uint16_t payload_len) {
	client_t *cp = clients;

	while (cp) {
		if (cp != from_client) {
			srf_ip_conn_packet_hmac_add(cp->token, config_server_password_str, packet, payload_len);

			server_sock_send((uint8_t *)packet, data_len, &cp->from_addr);
		}
		cp = cp->next;
	}
}

void client_process(void) {
	client_t *cp;
	ignored_ip_t *ip;
	char s[INET6_ADDRSTRLEN];
	static time_t last_client_list_log_at = 0;

	cp = clients_login;
	while (cp) {
		if (time(NULL) - cp->last_valid_packet_got_at >= config_client_login_timeout_sec) {
			syslog(LOG_INFO, "client %u %s:%u: login timeout\n", cp->client_id,
				inet_ntop(cp->from_addr.sa_family, sock_get_in_addr(&cp->from_addr), s, sizeof(s)),
				sock_get_port(&cp->from_addr));

			client_login_delete(cp, 1);
			cp = clients_login;
			continue;
		}
		cp = cp->next;
	}

	cp = clients;
	while (cp) {
		if (time(NULL) - cp->last_valid_packet_got_at >= config_client_timeout_sec) {
			syslog(LOG_INFO, "client %u %s:%u: timeout\n", cp->client_id,
				inet_ntop(cp->from_addr.sa_family, sock_get_in_addr(&cp->from_addr), s, sizeof(s)),
				sock_get_port(&cp->from_addr));

			client_delete(cp);
			cp = clients;
			continue;
		}
		cp = cp->next;
	}

	ip = client_ignored_ips;
	while (ip) {
		if (time(NULL) - ip->added_at >= config_auth_fail_ip_ignore_sec) {
			syslog(LOG_INFO, "client: removing ip %s from ignore list\n",
				inet_ntop(ip->addr.sa_family, sock_get_in_addr(&ip->addr), s, sizeof(s)));

			client_ignored_ip_delete(ip);
			ip = client_ignored_ips;
			continue;
		}
		ip = ip->next;
	}

	if (time(NULL)-last_client_list_log_at > 30) {
		last_client_list_log_at = time(NULL);

		if (clients == NULL)
			syslog(LOG_INFO, "client: no connected clients\n");
		else {
			syslog(LOG_INFO, "client: connected clients:\n");
			cp = clients;
			while (cp) {
				syslog(LOG_INFO, "  %10u %s:%u\n", cp->client_id,
					inet_ntop(cp->from_addr.sa_family, sock_get_in_addr(&cp->from_addr), s, sizeof(s)),
					sock_get_port(&cp->from_addr));

				cp = cp->next;
			}
		}
	}
}
