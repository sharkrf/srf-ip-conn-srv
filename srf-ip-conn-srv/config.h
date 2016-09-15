#ifndef CONFIG_H_
#define CONFIG_H_

#include "types.h"

#include <srf-ip-conn/common/srf-ip-conn-packet.h>

#include <netinet/in.h>

extern uint16_t config_port;
extern flag_t config_ipv4_only;
extern char config_bind_ip_str[INET6_ADDRSTRLEN];
extern uint16_t config_max_clients;
extern uint16_t config_client_login_timeout_sec;
extern uint16_t config_client_timeout_sec;
extern char config_server_password_str[SRF_IP_CONN_MAX_PASSWORD_LENGTH];
extern uint16_t config_auth_fail_ip_ignore_sec;
extern char config_pidfile_str[255];
extern char config_api_socket_file_str[255];
extern char config_server_name_str[255];
extern char config_server_desc_str[255];
extern char config_server_contact_str[255];
extern uint16_t config_max_lastheard_entry_count;

flag_t config_read(char *filename);

#endif
