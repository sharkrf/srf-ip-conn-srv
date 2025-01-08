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

#include "config.h"

#include <json.h>

#include <string.h>
#include <syslog.h>
#include <stdlib.h>

uint16_t config_port = 65100;
flag_t config_ipv4_only = 1;
char config_bind_ip_str[INET6_ADDRSTRLEN] = {0,};
uint16_t config_max_clients = 1000;
uint16_t config_client_login_timeout_sec = 10;
uint16_t config_client_timeout_sec = 30;
char config_server_password_str[SRF_IP_CONN_MAX_PASSWORD_LENGTH] = {0,};
uint16_t config_auth_fail_ip_ignore_sec = 5;
char config_pidfile_str[255] = {0,};
char config_api_socket_file_str[255] = "/tmp/srf-ip-conn-srv.socket";
char config_server_name_str[255] = "SharkRF IP Connector Protocol Server";
char config_server_desc_str[255] = {0,};
char config_server_contact_str[255] = {0,};
uint16_t config_max_lastheard_entry_count = 30;
uint16_t config_max_api_clients = 100;
uint16_t config_client_call_timeout_sec = 3;
uint16_t config_client_status_syslog_interval_sec = 30;
flag_t config_allow_simultaneous_calls = 0;
char config_banlist_file_str[255] = {0,};
flag_t config_allow_data_raw = 0;
flag_t config_allow_data_dmr = 1;
flag_t config_allow_data_dstar = 1;
flag_t config_allow_data_c4fm = 1;
flag_t config_allow_data_nxdn = 1;
flag_t config_allow_data_p25 = 1;

flag_t config_read(char *filename) {
	FILE *f;
	long fsize;
	char *buf;
	jsmn_parser json_parser;
	jsmntok_t *tok;
	int json_entry_count;
	int i;
	char port_str[6] = {0,};
	char ipv4_only_str[2] = {0,};
	char bind_ip_str[INET6_ADDRSTRLEN] = {0,};
	char max_clients_str[6] = {0,};
	char client_login_timeout_sec_str[6] = {0,};
	char client_timeout_sec_str[6] = {0,};
	char server_password_str[SRF_IP_CONN_MAX_PASSWORD_LENGTH] = {0,};
	char auth_fail_ip_ignore_sec_str[6] = {0,};
	char pidfile_str[255] = {0,};
	char api_socket_file_str[255] = {0,};
	char server_name_str[255] = {0,};
	char server_desc_str[255] = {0,};
	char server_contact_str[255] = {0,};
	char max_lastheard_entry_count_str[6] = {0,};
	char max_api_clients_str[6] = {0,};
	char client_call_timeout_sec_str[6] = {0,};
	char client_status_syslog_interval_sec_str[6] = {0,};
	char allow_simultaneous_calls_str[2] = {0,};
	char banlist_file_str[255] = {0,};
	char allow_data_raw_str[2] = {0,};
	char allow_data_dmr_str[2] = {0,};
	char allow_data_dstar_str[2] = {0,};
	char allow_data_c4fm_str[2] = {0,};
	char allow_data_nxdn_str[2] = {0,};
	char allow_data_p25_str[2] = {0,};

	f = fopen(filename, "r");
	if (f == NULL) {
		syslog(LOG_ERR, "config: can't open config file for reading: %s\n", filename);
		return 0;
	}

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	rewind(f);

	buf = (char *)malloc(fsize);
	if (buf == NULL) {
		fclose(f);
		syslog(LOG_ERR, "config: can't allocate memory for config file reading: %s\n", filename);
		return 0;
	}

	if (fread(buf, 1, fsize, f) != fsize) {
		fclose(f);
		free(buf);
		syslog(LOG_ERR, "config: error reading file: %s\n", filename);
	}
	fclose(f);

	jsmn_init(&json_parser);
	json_entry_count = jsmn_parse(&json_parser, buf, fsize, NULL, 0);
	tok = calloc(json_entry_count, sizeof(jsmntok_t));
	if (tok == NULL) {
		free(buf);
		syslog(LOG_ERR, "config: error allocating JSON tokens for file: %s\n", filename);
		return 0;
	}
	jsmn_init(&json_parser);
	json_entry_count = jsmn_parse(&json_parser, buf, fsize, tok, json_entry_count);
	if (json_entry_count < 1 || tok[0].type != JSMN_OBJECT) {
		free(tok);
		free(buf);
		syslog(LOG_ERR, "config: error parsing file: %s\n", filename);
		return 0;
	}

	for (i = 1; i < json_entry_count-1; i++) {
		if (json_compare_tok_key(buf, &tok[i], "port")) {
			json_get_value(buf, &tok[i+1], port_str, sizeof(port_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "ipv4-only")) {
			json_get_value(buf, &tok[i+1], ipv4_only_str, sizeof(ipv4_only_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "bind-ip")) {
			json_get_value(buf, &tok[i+1], bind_ip_str, sizeof(bind_ip_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "max-clients")) {
			json_get_value(buf, &tok[i+1], max_clients_str, sizeof(max_clients_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "client-login-timeout-sec")) {
			json_get_value(buf, &tok[i+1], client_login_timeout_sec_str, sizeof(client_login_timeout_sec_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "client-timeout-sec")) {
			json_get_value(buf, &tok[i+1], client_timeout_sec_str, sizeof(client_login_timeout_sec_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "server-password")) {
			json_get_value(buf, &tok[i+1], server_password_str, sizeof(server_password_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "auth-fail-ip-ignore-sec")) {
			json_get_value(buf, &tok[i+1], auth_fail_ip_ignore_sec_str, sizeof(auth_fail_ip_ignore_sec_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "pidfile")) {
			json_get_value(buf, &tok[i+1], pidfile_str, sizeof(pidfile_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "api-socket-file")) {
			json_get_value(buf, &tok[i+1], api_socket_file_str, sizeof(api_socket_file_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "server-name")) {
			json_get_value(buf, &tok[i+1], server_name_str, sizeof(server_name_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "server-desc")) {
			json_get_value(buf, &tok[i+1], server_desc_str, sizeof(server_desc_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "server-contact")) {
			json_get_value(buf, &tok[i+1], server_contact_str, sizeof(server_contact_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "max-lastheard-entry-count")) {
			json_get_value(buf, &tok[i+1], max_lastheard_entry_count_str, sizeof(max_lastheard_entry_count_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "max-api-clients")) {
			json_get_value(buf, &tok[i+1], max_api_clients_str, sizeof(max_api_clients_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "client-call-timeout-sec")) {
			json_get_value(buf, &tok[i+1], client_call_timeout_sec_str, sizeof(client_call_timeout_sec_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "client-status-syslog-interval-sec")) {
			json_get_value(buf, &tok[i+1], client_status_syslog_interval_sec_str, sizeof(client_status_syslog_interval_sec_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-simultaneous-calls")) {
			json_get_value(buf, &tok[i+1], allow_simultaneous_calls_str, sizeof(allow_simultaneous_calls_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "banlist-file")) {
			json_get_value(buf, &tok[i+1], banlist_file_str, sizeof(banlist_file_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-data-raw")) {
			json_get_value(buf, &tok[i+1], allow_data_raw_str, sizeof(allow_data_raw_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-data-dmr")) {
			json_get_value(buf, &tok[i+1], allow_data_dmr_str, sizeof(allow_data_dmr_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-data-dstar")) {
			json_get_value(buf, &tok[i+1], allow_data_dstar_str, sizeof(allow_data_dstar_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-data-c4fm")) {
			json_get_value(buf, &tok[i+1], allow_data_c4fm_str, sizeof(allow_data_c4fm_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-data-nxdn")) {
			json_get_value(buf, &tok[i+1], allow_data_nxdn_str, sizeof(allow_data_nxdn_str));
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "allow-data-p25")) {
			json_get_value(buf, &tok[i+1], allow_data_p25_str, sizeof(allow_data_p25_str));
			i++;
		} else {
			syslog(LOG_ERR, "config: unexpected key at %u\n", tok[i].start);
			free(tok);
			free(buf);
			return 0;
		}
	}

	free(tok);
	free(buf);

	if (port_str[0])
		config_port = atoi(port_str);
	if (ipv4_only_str[0])
		config_ipv4_only = (ipv4_only_str[0] == '1');
	if (bind_ip_str[0])
		strncpy(config_bind_ip_str, bind_ip_str, sizeof(config_bind_ip_str));
	if (max_clients_str[0])
		config_max_clients = atoi(max_clients_str);
	if (client_login_timeout_sec_str[0])
		config_client_login_timeout_sec = atoi(client_login_timeout_sec_str);
	if (client_timeout_sec_str[0])
		config_client_timeout_sec = atoi(client_timeout_sec_str);
	if (server_password_str[0])
		strncpy(config_server_password_str, server_password_str, sizeof(config_server_password_str));
	if (auth_fail_ip_ignore_sec_str[0])
		config_auth_fail_ip_ignore_sec = atoi(auth_fail_ip_ignore_sec_str);
	if (pidfile_str[0])
		strncpy(config_pidfile_str, pidfile_str, sizeof(config_pidfile_str));
	if (api_socket_file_str[0])
		strncpy(config_api_socket_file_str, api_socket_file_str, sizeof(config_api_socket_file_str));
	if (server_name_str[0])
		strncpy(config_server_name_str, server_name_str, sizeof(config_server_name_str));
	if (server_desc_str[0])
		strncpy(config_server_desc_str, server_desc_str, sizeof(config_server_desc_str));
	if (server_contact_str[0])
		strncpy(config_server_contact_str, server_contact_str, sizeof(config_server_contact_str));
	if (max_lastheard_entry_count_str[0])
		config_max_lastheard_entry_count = atoi(max_lastheard_entry_count_str);
	if (max_api_clients_str[0])
		config_max_api_clients = atoi(max_api_clients_str);
	if (client_call_timeout_sec_str[0])
		config_client_call_timeout_sec = atoi(client_call_timeout_sec_str);
	if (client_status_syslog_interval_sec_str[0])
		config_client_status_syslog_interval_sec = atoi(client_status_syslog_interval_sec_str);
	if (allow_simultaneous_calls_str[0])
		config_allow_simultaneous_calls = (allow_simultaneous_calls_str[0] == '1');
	if (banlist_file_str[0])
		strncpy(config_banlist_file_str, banlist_file_str, sizeof(config_banlist_file_str));
	if (allow_data_raw_str[0])
		config_allow_data_raw = (allow_data_raw_str[0] == '1');
	if (allow_data_dmr_str[0])
		config_allow_data_dmr = (allow_data_dmr_str[0] == '1');
	if (allow_data_dstar_str[0])
		config_allow_data_dstar = (allow_data_dstar_str[0] == '1');
	if (allow_data_c4fm_str[0])
		config_allow_data_c4fm = (allow_data_c4fm_str[0] == '1');
	if (allow_data_nxdn_str[0])
		config_allow_data_nxdn = (allow_data_nxdn_str[0] == '1');
	if (allow_data_p25_str[0])
		config_allow_data_p25 = (allow_data_p25_str[0] == '1');

	return 1;
}
