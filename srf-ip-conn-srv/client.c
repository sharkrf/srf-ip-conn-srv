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

client_t *client_in_call = NULL;
time_t client_in_call_started_at;

// Searches for the given ID amongst the clients.
static client_t *client_search_id(uint32_t client_id) {
	client_t *cp = clients;

	while (cp) {
		if (cp->client_id == client_id)
			return cp;
		cp = cp->next;
	}
	return NULL;
}

char *client_build_config_json(uint32_t client_id) {
	char *res;
	int res_size = 900;
	client_t *cp;

	res = (char *)calloc(res_size+1, 1); // +1 so response will be always null-terminated.
	if (res == NULL)
		return NULL;

	cp = client_search_id(client_id);
	if (cp == NULL || !cp->got_config) {
		snprintf(res, res_size, "{\"req\":\"client-config\",\"id\":%u,\"got-config\":0}", client_id);
	} else {
		snprintf(res, res_size, "{\"req\":\"client-config\",\"id\":%u,\"got-config\":1,\"operator-callsign\":\"%s\","
			"\"hw-manufacturer\":\"%s\",\"hw-model\":\"%s\",\"hw-version\":\"%s\",\"sw-version\":\"%s\","
			"\"rx-freq\":%u,\"tx-freq\":%u,\"tx-power\":%u,\"latitude\":\"%f\",\"longitude\":\"%f\","
			"\"height-agl\":\"%d\",\"location\":\"%s\",\"description\":\"%s\"}",
			client_id, cp->config.operator_callsign, cp->config.hw_manufacturer, cp->config.hw_model,
			cp->config.hw_version, cp->config.sw_version, ntohl(cp->config.rx_freq), ntohl(cp->config.tx_freq),
			cp->config.tx_power, cp->config.latitude, cp->config.longitude, ntohs(cp->config.height_agl),
			cp->config.location, cp->config.description);
	}

	return res;
}

char *client_build_list_json(void) {
	char *res;
	int res_size = 32+clients_count*100;
	int res_remaining_size = res_size;
	int resp;
	int printed_chars;
	client_t *cp = clients;
	int i = 0;

	res = (char *)calloc(res_size+1, 1); // +1 so response will be always null-terminated.
	if (res == NULL)
		return NULL;

	printed_chars = snprintf(res, res_size, "{\"req\":\"client-list\",\"list\":[");
	res_remaining_size -= printed_chars;
	resp = printed_chars;
	while (cp) {
		if (i++ > 0) {
			printed_chars = snprintf(res+resp, res_remaining_size, ",");
			res_remaining_size -= printed_chars;
			if (res_remaining_size <= 0)
				break;
			resp += printed_chars;
		}

		printed_chars = snprintf(res+resp, res_remaining_size,
				"{\"id\":%u,\"last-pkt-at\":%lu,\"got-config\":%u,\"callsign\":\"%s\"}",
				cp->client_id, cp->last_valid_packet_got_at, cp->got_config, cp->config.operator_callsign);
		res_remaining_size -= printed_chars;
		if (res_remaining_size <= 0)
			break;
		resp += printed_chars;

		cp = cp->next;
	}
	if (res_remaining_size > 0)
		snprintf(res+resp, res_remaining_size, "]}");

	return res;
}

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
	newclient->rx_seqnum = newclient->tx_seqnum = 0xffffffff;

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

void client_broadcast(client_t *from_client, srf_ip_conn_packet_t *packet, uint16_t data_len,
		uint16_t payload_len, uint32_t missing_packets_num)
{
	client_t *cp = clients;

	while (cp) {
		if (cp != from_client) {
			cp->tx_seqnum = cp->tx_seqnum + 1 + missing_packets_num;
			switch (packet->header.packet_type) {
				case SRF_IP_CONN_PACKET_TYPE_DATA_RAW: packet->data_raw.seq_no = htonl(cp->tx_seqnum); break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_DMR: packet->data_dmr.seq_no = htonl(cp->tx_seqnum); break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_DSTAR: packet->data_dstar.seq_no = htonl(cp->tx_seqnum); break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_C4FM: packet->data_c4fm.seq_no = htonl(cp->tx_seqnum); break;
			}
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

	if (client_in_call && time(NULL)-client_in_call->last_data_packet_at >= config_client_call_timeout_sec) {
		syslog(LOG_INFO, "client: %u call timeout, duration %lu sec.\n", client_in_call->client_id,
				time(NULL)-client_in_call_started_at);
		client_in_call = NULL;
	}
}
