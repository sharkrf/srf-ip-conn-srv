#include "api.h"
#include "config.h"
#include "client.h"
#include "githash.h"

#include <json.h>

#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

static api_client_t *api_clients = NULL;

static void api_client_delete(api_client_t *client) {
	if (client->sock >= 0)
		close(client->sock);
	if (client->next)
		client->next->prev = client->prev;
	if (client->prev)
		client->prev->next = client->next;
	if (client == api_clients)
		api_clients = client->next;
	free(client);
}

static char *api_build_serverdetails_json(void) {
	extern time_t main_started_at;
	char *res;
	int res_size = 1000;

	res = (char *)calloc(res_size+1, 1); // +1 so response will be always null-terminated.
	if (res == NULL)
		return NULL;

	snprintf(res, res_size, "{\"req\":\"server-details\",\"name\":\"%s\",\"desc\":\"%s\",\"contact\":\"%s\","
			"\"uptime\":%lu,\"githash\":\"%s\"}", config_server_name_str, config_server_desc_str,
			config_server_contact_str, time(NULL)-main_started_at, GIT_SHA1);

	return res;
}

static void api_process_req(api_client_t *client, char *req, uint16_t req_size) {
	jsmn_parser json_parser;
	jsmntok_t tok[10];
	int json_entry_count;
	int i;
	char req_name_str[15] = {0,};
	char *reply = NULL;
	char client_id_str[11] = {0,};

	jsmn_init(&json_parser);
	json_entry_count = jsmn_parse(&json_parser, req, req_size, tok, sizeof(tok) / sizeof(tok[0]));
	if (json_entry_count < 1 || tok[0].type != JSMN_OBJECT) {
		syslog(LOG_ERR, "api: got invalid request from client %u\n", client->sock);
		return;
	}

	for (i = 1; i < json_entry_count-1; i++) {
		if (json_compare_tok_key(req, &tok[i], "req")) {
			json_get_value(req, &tok[i+1], req_name_str, sizeof(req_name_str));
			i++;
		} else if (json_compare_tok_key(req, &tok[i], "client-id")) {
			json_get_value(req, &tok[i+1], client_id_str, sizeof(client_id_str));
			i++;
		} else {
			syslog(LOG_ERR, "api: unexpected key in request from client %u at %u\n", client->sock, tok[i].start);
			return;
		}
	}

	if (memcmp(req_name_str, "client-list", 12) == 0)
		reply = client_build_list_json();
	if (memcmp(req_name_str, "server-details", 15) == 0)
		reply = api_build_serverdetails_json();
	if (memcmp(req_name_str, "client-config", 14) == 0)
		reply = client_build_config_json(atoi(client_id_str));

	if (reply != NULL) {
		send(client->sock, reply, strlen(reply), 0);
		free(reply);
	}
	api_client_delete(client);
}

// Returns the max. fd value.
int api_add_clients_to_fd_set(fd_set *fds) {
	api_client_t *ac = api_clients;
	int max = -1;

	while (ac) {
		FD_SET(ac->sock, fds);
		if (ac->sock > max)
			max = ac->sock;
		ac = ac->next;
	}
	return max;
}

void api_process_fd_set(fd_set *fds) {
	api_client_t *ac = api_clients;
	int received_bytes;
	char req_buf[512];

	while (ac) {
		if (FD_ISSET(ac->sock, fds)) {
			received_bytes = recv(ac->sock, &req_buf, sizeof(req_buf), 0);
			if (received_bytes >= 2) // As we expect a JSON struct, which has a mininum size of 2 chars.
				api_process_req(ac, req_buf, received_bytes);
		}
		ac = ac->next;
	}
}

static api_client_t *api_client_search(int sock) {
	api_client_t *ac = api_clients;

	while (ac) {
		if (ac->sock == sock)
			return ac;
		ac = ac->next;
	}
	return NULL;
}

flag_t api_client_add(int sock) {
	api_client_t *newclient;

	newclient = api_client_search(sock);
	if (newclient == NULL) {
		newclient = (api_client_t *)calloc(sizeof(api_client_t), 1);
		if (newclient == NULL)
			return 0;
	} else {
		// If the client already exists, we only update the timestamp.
		newclient->last_valid_packet_got_at = time(NULL);
		return 1;
	}

	newclient->sock = sock;
	newclient->last_valid_packet_got_at = time(NULL);

	if (api_clients == NULL)
		api_clients = newclient;
	else {
		// Inserting to the beginning of the ignored IP list.
		api_clients->prev = newclient;
		newclient->next = api_clients;
		api_clients = newclient;
	}
	return 1;
}

void api_process(void) {
	api_client_t *cp;

	cp = api_clients;
	while (cp) {
		if (time(NULL) - cp->last_valid_packet_got_at >= config_client_timeout_sec) {
			//syslog(LOG_INFO, "api: client %u timeout\n", cp->sock);
			api_client_delete(cp);
			cp = api_clients;
			continue;
		}
		cp = cp->next;
	}
}

// Returns the socket, or -1 on error.
int api_init(char *api_socket_file) {
	int sock, len;
	struct sockaddr_un local;

	if (strlen(api_socket_file) == 0)
		return -1;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		syslog(LOG_ERR, "api: unix socket create error\n");
        return -1;
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, api_socket_file);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(sock, (struct sockaddr *)&local, len) == -1) {
    	close(sock);
		syslog(LOG_ERR, "api: unix socket bind error\n");
		return -1;
    }

    if (listen(sock, 5) == -1) {
    	close(sock);
		syslog(LOG_ERR, "api: unix socket listen error\n");
		return -1;
    }

    return sock;
}
