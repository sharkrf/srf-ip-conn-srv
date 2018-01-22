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
extern uint16_t config_max_api_clients;
extern uint16_t config_client_call_timeout_sec;
extern uint16_t config_client_status_syslog_interval_sec;
extern flag_t config_allow_simultaneous_calls;
extern char config_banlist_file_str[255];

flag_t config_read(char *filename);

#endif
