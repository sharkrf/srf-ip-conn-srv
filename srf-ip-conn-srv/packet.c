#include "packet.h"
#include "server-sock.h"
#include "config.h"
#include "client.h"

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

static void packet_process_raw(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_raw_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_raw_payload_t)))
		return;

	client->last_valid_packet_got_at = time(NULL);
	client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_raw_payload_t));
}

static void packet_process_dmr(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_dmr_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_dmr_payload_t)))
		return;

	client->last_valid_packet_got_at = time(NULL);
	client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_dmr_payload_t));
}

static void packet_process_dstar(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_dstar_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_dstar_payload_t)))
		return;

	client->last_valid_packet_got_at = time(NULL);
	client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_dstar_payload_t));
}

static void packet_process_c4fm(server_sock_received_packet_t *received_packet) {
	srf_ip_conn_packet_t *packet = (srf_ip_conn_packet_t *)received_packet->buf;
	client_t *client;

	if (received_packet->received_bytes != sizeof(srf_ip_conn_packet_header_t) + sizeof(srf_ip_conn_data_c4fm_payload_t))
		return;

	client = client_search(&received_packet->from_addr);
	if (!client)
		return;

	if (!srf_ip_conn_packet_hmac_check(client->token, config_server_password_str, packet, sizeof(srf_ip_conn_data_dstar_payload_t)))
		return;

	client->last_valid_packet_got_at = time(NULL);
	client_broadcast(client, packet, received_packet->received_bytes, sizeof(srf_ip_conn_data_c4fm_payload_t));
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
			}
			break;
	}
}
