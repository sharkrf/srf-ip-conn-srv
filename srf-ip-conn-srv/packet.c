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

#include "packet.h"
#include "server-sock.h"
#include "config.h"
#include "client.h"
#include "lastheard.h"
#include "banlist.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <syslog.h>

static void packet_process_login(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	srf_ip_conn_packet_t answer_packet;
	client_t *newclient;
	char s[INET6_ADDRSTRLEN];

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_login_payload_t))
		return;

	if (clients_count+clients_login_count+1 > config_max_clients) {
		syslog(LOG_INFO, "packet: client %u %s:%u: ignoring login packet, server full\n", ntohl(packet->login.client_id),
				inet_ntop(received_packet->from_addr.sa_family, sock_get_in_addr(&received_packet->from_addr), s, sizeof(s)),
				sock_get_port(&received_packet->from_addr));
		return;
	}

	if (banlist_is_banned_client_id(ntohl(packet->login.client_id))) {
		syslog(LOG_INFO, "packet: client %u %s:%u: ignoring login packet, client id banned\n", ntohl(packet->login.client_id),
				inet_ntop(received_packet->from_addr.sa_family, sock_get_in_addr(&received_packet->from_addr), s, sizeof(s)),
				sock_get_port(&received_packet->from_addr));
		return;
	}
	if (banlist_is_banned_client_ip(&received_packet->from_addr)) {
		syslog(LOG_INFO, "packet: client %u %s:%u: ignoring login packet, client ip banned\n", ntohl(packet->login.client_id),
				inet_ntop(received_packet->from_addr.sa_family, sock_get_in_addr(&received_packet->from_addr), s, sizeof(s)),
				sock_get_port(&received_packet->from_addr));
		return;
	}

	newclient = client_login_add(ntohl(packet->login.client_id), &received_packet->from_addr);
	if (newclient == NULL)
		return;

	syslog(LOG_INFO, "packet: client %u %s:%u: new login\n", newclient->client_id,
		inet_ntop(newclient->from_addr.sa_family, sock_get_in_addr(&newclient->from_addr), s, sizeof(s)),
		sock_get_port(&newclient->from_addr));

	srf_ip_conn_packet_init(&answer_packet.header, SRF_IP_CONN_PACKET_TYPE_TOKEN);
	memcpy(answer_packet.token.token, newclient->token, SRF_IP_CONN_TOKEN_LENGTH);
	server_sock_send((uint8_t *)&answer_packet, sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_token_payload_t), &newclient->from_addr);
}

static void packet_process_auth(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	srf_ip_conn_packet_t answer_packet;
	client_t *client;
	int i;
	char s[INET6_ADDRSTRLEN];

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_auth_payload_t))
		return;

	client = client_login_search(&received_packet->from_addr);
	if (!client)
		return;

	// Preventing quick login retries.
	if (client_ignored_ip_search(&client->from_addr)) {
		syslog(LOG_INFO, "packet: client %u %s:%u: too many auth retries from ip, ignoring auth packet\n", client->client_id,
				inet_ntop(client->from_addr.sa_family, sock_get_in_addr(&client->from_addr), s, sizeof(s)),
				sock_get_port(&client->from_addr));
		client_login_delete(client, 1);
		return;
	}

	// If hash is not matching the HMAC we received in the auth packet, we send a nak.
	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_auth_payload_t))) {
		syslog(LOG_INFO, "packet: client %u %s:%u: auth fail, adding it's ip to the ignore list for %u seconds\n", client->client_id,
			inet_ntop(client->from_addr.sa_family, sock_get_in_addr(&client->from_addr), s, sizeof(s)),
			sock_get_port(&client->from_addr), config_auth_fail_ip_ignore_sec);

		client_ignored_ip_add(&client->from_addr);

		srf_ip_conn_packet_init(&answer_packet.header, SRF_IP_CONN_PACKET_TYPE_NAK);
		for (i = 0; i < sizeof(answer_packet.nak.random_data); i++)
			answer_packet.nak.random_data[i] = rand();
		answer_packet.nak.result = SRF_IP_CONN_NAK_RESULT_AUTH_INVALID_HMAC;
		srf_ip_conn_packet_hmac_add(client->token, config_server_password_str, &answer_packet, sizeof(srf_ip_conn_nak_payload_t));
		server_sock_send((uint8_t *)&answer_packet, sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_nak_payload_t), &client->from_addr);

		client_login_delete(client, 1);
		return;
	}

	if (clients_count+clients_login_count+1 > config_max_clients) {
		syslog(LOG_INFO, "packet: client %u %s:%u: can't login, server full\n", client->client_id,
				inet_ntop(client->from_addr.sa_family, sock_get_in_addr(&client->from_addr), s, sizeof(s)),
				sock_get_port(&client->from_addr));
		client_login_delete(client, 1);
		return;
	}

	syslog(LOG_INFO, "packet: client %u %s:%u: logged in\n", client->client_id,
		inet_ntop(client->from_addr.sa_family, sock_get_in_addr(&client->from_addr), s, sizeof(s)),
		sock_get_port(&client->from_addr));

	client_login_delete(client, 0);
	client_add(client);

	srf_ip_conn_packet_init(&answer_packet.header, SRF_IP_CONN_PACKET_TYPE_ACK);
	for (i = 0; i < sizeof(answer_packet.ack.random_data); i++)
		answer_packet.ack.random_data[i] = rand();
	answer_packet.ack.result = SRF_IP_CONN_ACK_RESULT_AUTH;
	srf_ip_conn_packet_hmac_add(client->token, config_server_password_str, &answer_packet, sizeof(srf_ip_conn_ack_payload_t));
	server_sock_send((uint8_t *)&answer_packet, sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_ack_payload_t), &client->from_addr);
}

static void packet_process_config(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	srf_ip_conn_packet_t answer_packet;
	client_t *client;
	int i;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_config_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_config_payload_t)))
		return;

	memcpy(&client->config, &packet->config, sizeof(srf_ip_conn_config_payload_t));
	client->got_config = 1;
	client->last_valid_packet_got_at = time(NULL);

	srf_ip_conn_packet_init(&answer_packet.header, SRF_IP_CONN_PACKET_TYPE_ACK);
	answer_packet.ack.result = SRF_IP_CONN_ACK_RESULT_CONFIG;
	for (i = 0; i < sizeof(answer_packet.ack.random_data); i++)
		answer_packet.ack.random_data[i] = rand();
	srf_ip_conn_packet_hmac_add(client->token, config_server_password_str, &answer_packet, sizeof(srf_ip_conn_ack_payload_t));
	server_sock_send((uint8_t *)&answer_packet, sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_ack_payload_t), &client->from_addr);
}

static void packet_process_ping(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	srf_ip_conn_packet_t answer_packet;
	client_t *client;
	int i;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_ping_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_ping_payload_t)))
		return;

	client->last_valid_packet_got_at = time(NULL);

	srf_ip_conn_packet_init(&answer_packet.header, SRF_IP_CONN_PACKET_TYPE_PONG);
	for (i = 0; i < sizeof(answer_packet.pong.random_data); i++)
		answer_packet.pong.random_data[i] = rand();
	srf_ip_conn_packet_hmac_add(client->token, config_server_password_str, &answer_packet, sizeof(srf_ip_conn_pong_payload_t));
	server_sock_send((uint8_t *)&answer_packet, sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_pong_payload_t), &client->from_addr);
}

static void packet_process_close(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	srf_ip_conn_packet_t answer_packet;
	client_t *client;
	int i;
	char s[INET6_ADDRSTRLEN];

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_close_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_close_payload_t)))
		return;

	srf_ip_conn_packet_init(&answer_packet.header, SRF_IP_CONN_PACKET_TYPE_ACK);
	answer_packet.ack.result = SRF_IP_CONN_ACK_RESULT_CLOSE;
	for (i = 0; i < sizeof(answer_packet.ack.random_data); i++)
		answer_packet.ack.random_data[i] = rand();
	srf_ip_conn_packet_hmac_add(client->token, config_server_password_str, &answer_packet, sizeof(srf_ip_conn_ack_payload_t));
	server_sock_send((uint8_t *)&answer_packet, sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_ack_payload_t), &client->from_addr);

	syslog(LOG_INFO, "packet: client %u %s:%u: logout\n", client->client_id,
		inet_ntop(client->from_addr.sa_family, sock_get_in_addr(&client->from_addr), s, sizeof(s)),
		sock_get_port(&client->from_addr));
	client_delete(client);
}

static uint32_t packet_get_missing_packet_count(uint32_t new_seq_no, uint32_t old_seq_no) {
	old_seq_no++;
	if (new_seq_no >= old_seq_no)
		return new_seq_no-old_seq_no;
	else
		return (0xffffffff-old_seq_no)+new_seq_no;
}

static void packet_process_raw(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;
	uint32_t rx_seqnum;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_raw_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_raw_payload_t)))
		return;

	client->last_data_packet_at = client->last_valid_packet_got_at = time(NULL);

	rx_seqnum = ntohl(packet->data_raw.seq_no);

	if (config_allow_data_raw) {
		if (client_in_call == NULL) {
			syslog(LOG_INFO, "packet: client %u raw call start, sid: %.8x\n", client->client_id,
				ntohl(packet->data_raw.call_session_id));
			client_in_call = client;
			client_in_call_started_at = time(NULL);
		}
		if (client_in_call == client || config_allow_simultaneous_calls) {
			lastheard_add("N/A", "N/A", 1, client->client_id, packet->data_raw.call_session_id, LASTHEARD_MODE_RAW, time(NULL)-client_in_call_started_at);
			client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_raw_payload_t),
				packet_get_missing_packet_count(rx_seqnum, client->rx_seqnum));
		}
	}
	client->rx_seqnum = rx_seqnum;
}

static void packet_process_dmr(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;
	uint32_t rx_seqnum;
	flag_t data_pkt;
	char to[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1];
	char from[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1];

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_dmr_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_dmr_payload_t)))
		return;

	client->last_data_packet_at = client->last_valid_packet_got_at = time(NULL);

	rx_seqnum = ntohl(packet->data_dmr.seq_no);

	if (config_allow_data_dmr) {
		if (client_in_call == NULL) {
			syslog(LOG_INFO, "packet: client dmr %u call start, sid: %.8x\n", client->client_id,
				ntohl(packet->data_dmr.call_session_id));
			client_in_call = client;
			client_in_call_started_at = time(NULL);
		}
		data_pkt = 0;
		switch (packet->data_dmr.slot_type) {
			case SRF_IP_CONN_DATA_DMR_SLOT_TYPE_CSBK:
			case SRF_IP_CONN_DATA_DMR_SLOT_TYPE_DATA_HEADER:
			case SRF_IP_CONN_DATA_DMR_SLOT_TYPE_RATE_12_DATA:
			case SRF_IP_CONN_DATA_DMR_SLOT_TYPE_RATE_34_DATA:
				data_pkt = 1;
		}
		if (client_in_call == client || config_allow_simultaneous_calls || data_pkt) {
			snprintf(to, sizeof(to), "%u", (unsigned int)(packet->data_dmr.dst_id[0] << 16) | (packet->data_dmr.dst_id[1] << 8) | packet->data_dmr.dst_id[2]);
			snprintf(from, sizeof(from), "%u", (unsigned int)(packet->data_dmr.src_id[0] << 16) | (packet->data_dmr.src_id[1] << 8) | packet->data_dmr.src_id[2]);
			lastheard_add(to, from, packet->data_dmr.call_type, client->client_id, packet->data_dmr.call_session_id,
				LASTHEARD_MODE_DMR, time(NULL)-client_in_call_started_at);
			client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_dmr_payload_t),
				packet_get_missing_packet_count(rx_seqnum, client->rx_seqnum));

			if (packet->data_dmr.slot_type == SRF_IP_CONN_DATA_DMR_SLOT_TYPE_TERMINATOR_WITH_LC || data_pkt) {
				syslog(LOG_INFO, "packet: client %u dmr call end, sid: %.8x, duration %lu sec.\n", client->client_id,
					ntohl(packet->data_dmr.call_session_id), time(NULL)-client_in_call_started_at);
				client_in_call = NULL;
			}
		}
	}
	client->rx_seqnum = rx_seqnum;
}

static void packet_process_dstar(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;
	uint32_t rx_seqnum;
	int i;
	flag_t got_terminator;
	char to[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1] = { 'N', '/', 'A', 0, };
	char from[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1] = { 'N', '/', 'A', 0, };

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_dstar_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_dstar_payload_t)))
		return;

	client->last_data_packet_at = client->last_valid_packet_got_at = time(NULL);

	rx_seqnum = ntohl(packet->data_dstar.seq_no);

	if (config_allow_data_dstar) {
		if (client_in_call == NULL) {
			syslog(LOG_INFO, "packet: client %u dstar call start, sid: %.8x\n", client->client_id,
				ntohl(packet->data_dstar.call_session_id));
			client_in_call = client;
			client_in_call_started_at = time(NULL);
		}
		if (client_in_call == client || config_allow_simultaneous_calls) {
			got_terminator = 0;
			for (i = 0; i < packet->data_dstar.storage.packet_count; i++) {
				if (packet->data_dstar.storage.packet_types[i] == SRF_IP_CONN_DATA_DSTAR_PACKET_TYPE_HEADER) {
					memcpy(to, packet->data_dstar.storage.decoded_header.dst_callsign, min(sizeof(to),
						sizeof(packet->data_dstar.storage.decoded_header.dst_callsign)));
					to[sizeof(to)-1] = 0;
					memcpy(from, packet->data_dstar.storage.decoded_header.src_callsign, min(sizeof(from),
						sizeof(packet->data_dstar.storage.decoded_header.src_callsign)));
					from[sizeof(from)-1] = 0;
				}

				if (packet->data_dstar.storage.packet_types[i] == SRF_IP_CONN_DATA_DSTAR_PACKET_TYPE_TERMINATOR)
					got_terminator = 1;
				else
					got_terminator = 0;
			}

			lastheard_add(to, from, 1, client->client_id, ntohl(packet->data_dstar.call_session_id), LASTHEARD_MODE_DSTAR,
				time(NULL)-client_in_call_started_at);
			client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_dstar_payload_t),
				packet_get_missing_packet_count(rx_seqnum, client->rx_seqnum));

			if (got_terminator) {
				syslog(LOG_INFO, "packet: client %u dstar call end, sid: %.8x, duration %lu sec.\n", client->client_id,
					ntohl(packet->data_dstar.call_session_id), time(NULL)-client_in_call_started_at);
				client_in_call = NULL;
			}
		}
	}
	client->rx_seqnum = rx_seqnum;
}

static void packet_process_c4fm(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;
	uint32_t rx_seqnum;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_c4fm_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_c4fm_payload_t)))
		return;

	client->last_data_packet_at = client->last_valid_packet_got_at = time(NULL);

	rx_seqnum = ntohl(packet->data_c4fm.seq_no);

	if (config_allow_data_c4fm) {
		if (client_in_call == NULL || config_allow_simultaneous_calls) {
			syslog(LOG_INFO, "packet: client %u c4fm call start, sid: %.8x\n", client->client_id,
				ntohl(packet->data_c4fm.call_session_id));
			client_in_call = client;
			client_in_call_started_at = time(NULL);
		}
		if (client_in_call == client) {
			packet->data_c4fm.dst_callsign[sizeof(packet->data_c4fm.dst_callsign)-1] = 0;
			packet->data_c4fm.src_callsign[sizeof(packet->data_c4fm.src_callsign)-1] = 0;
			lastheard_add((char *)packet->data_c4fm.dst_callsign, (char *)packet->data_c4fm.src_callsign, packet->data_c4fm.call_type,
				client->client_id, ntohl(packet->data_c4fm.call_session_id), LASTHEARD_MODE_C4FM, time(NULL)-client_in_call_started_at);
			client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_c4fm_payload_t),
				packet_get_missing_packet_count(rx_seqnum, client->rx_seqnum));

			if (packet->data_c4fm.packet_type == SRF_IP_CONN_DATA_C4FM_PACKET_TYPE_TERMINATOR) {
				syslog(LOG_INFO, "packet: client %u c4fm call end, sid: %.8x, duration %lu sec.\n", client->client_id,
					ntohl(packet->data_c4fm.call_session_id), time(NULL)-client_in_call_started_at);
				client_in_call = NULL;
			}
		}
	}
	client->rx_seqnum = rx_seqnum;
}

static void packet_process_nxdn(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;
	uint32_t rx_seqnum;
	flag_t data_pkt;
	char to[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1];
	char from[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1];

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_nxdn_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_nxdn_payload_t)))
		return;

	client->last_data_packet_at = client->last_valid_packet_got_at = time(NULL);

	rx_seqnum = ntohl(packet->data_nxdn.seq_no);

	if (config_allow_data_nxdn) {
		if (client_in_call == NULL) {
			syslog(LOG_INFO, "packet: client nxdn %u call start, sid: %.8x\n", client->client_id,
				ntohl(packet->data_nxdn.call_session_id));
			client_in_call = client;
			client_in_call_started_at = time(NULL);
		}

		data_pkt = (packet->data_nxdn.packet_type == SRF_IP_CONN_DATA_NXDN_PACKET_TYPE_DATA);

		if (client_in_call == client || config_allow_simultaneous_calls || data_pkt) {
			snprintf(to, sizeof(to), "%u", packet->data_nxdn.dst_id);
			snprintf(from, sizeof(from), "%u", packet->data_nxdn.src_id);
			lastheard_add(to, from, packet->data_nxdn.call_type, client->client_id, packet->data_nxdn.call_session_id,
				LASTHEARD_MODE_NXDN, time(NULL)-client_in_call_started_at);
			client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_nxdn_payload_t),
				packet_get_missing_packet_count(rx_seqnum, client->rx_seqnum));

			if (packet->data_nxdn.packet_type == SRF_IP_CONN_DATA_NXDN_PACKET_TYPE_TERMINATOR || data_pkt) {
				syslog(LOG_INFO, "packet: client %u nxdn call end, sid: %.8x, duration %lu sec.\n", client->client_id,
					ntohl(packet->data_nxdn.call_session_id), time(NULL)-client_in_call_started_at);
				client_in_call = NULL;
			}
		}
	}
	client->rx_seqnum = rx_seqnum;
}

void packet_process(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_header_t *header = (srf_ip_conn_packet_header_t *)received_packet->buf;

	switch (header->version) {
		case 0:
			switch (header->packet_type) {
				case SRF_IP_CONN_PACKET_TYPE_LOGIN:
					packet_process_login(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_AUTH:
					packet_process_auth(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_CONFIG:
					packet_process_config(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_PING:
					packet_process_ping(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_CLOSE:
					packet_process_close(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_RAW:
					packet_process_raw(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_DMR:
					packet_process_dmr(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_DSTAR:
					packet_process_dstar(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_C4FM:
					packet_process_c4fm(received_packet);
					break;
				case SRF_IP_CONN_PACKET_TYPE_DATA_NXDN:
					packet_process_nxdn(received_packet);
					break;
			}
			break;
	}
}
