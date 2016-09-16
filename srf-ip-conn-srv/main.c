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

#include "types.h"
#include "config.h"
#include "packet.h"
#include "client.h"
#include "api.h"

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <time.h>

#define MAIN_FORK_RESULT_OK				0
#define MAIN_FORK_RESULT_OK_PARENTEXIT	1
#define MAIN_FORK_RESULT_ERROR			2
typedef int main_fork_result_t;

static struct {
	flag_t run_in_foreground	: 1;
	flag_t sigexit				: 1;
	flag_t sigint_received		: 1;
	flag_t sigterm_received		: 1;
} main_flags = {0,};

static char *main_configfile = "config.json";
static int main_server_sock_fd;
static int main_api_sock_fd;
time_t main_started_at;

static void main_sighandler(int signal) {
	switch (signal) {
		case SIGINT:
			if (main_flags.sigexit) {
				syslog(LOG_NOTICE, "main: SIGINT received again, exiting\n");
				exit(0);
			}
			main_flags.sigint_received = 1;
			main_flags.sigexit = 1;
			break;
		case SIGTERM:
			if (main_flags.sigexit) {
				syslog(LOG_NOTICE, "main: SIGTERM received again, exiting\n");
				exit(0);
			}
			main_flags.sigterm_received = 1;
			main_flags.sigexit = 1;
			break;
	}
}

static main_fork_result_t main_daemonize(void) {
	pid_t sid;
	pid_t childpid = fork();

	if (childpid > 0) // Fork successful, closing the parent
		return MAIN_FORK_RESULT_OK_PARENTEXIT;

	if (childpid < 0) { // Fork error, child not created
		syslog(LOG_ERR, "main error: can't fork to the background\n");
		return MAIN_FORK_RESULT_ERROR;
	}

	// This section is only executed by the forked child

	// Creating a new SID
	sid = setsid();
	if (sid < 0) {
		syslog(LOG_ERR, "main error: can't create sid, fork error\n");
		return MAIN_FORK_RESULT_ERROR;
	}

	// Closing standard outputs, inputs as they isn't needed.
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	return MAIN_FORK_RESULT_OK;
}

static void main_writepidfile(void) {
	FILE *f = fopen(config_pidfile_str, "w");
	if (f) {
		fprintf(f, "%d\n", getpid());
		fclose(f);
	} else
		syslog(LOG_ERR, "main: can't write pidfile %s\n", config_pidfile_str);
}

static void main_printversion(void) {
	printf("SharkRF IP Connector server by Norbert, HA2NON <ha2non@sharkrf.com>\n");
}

static flag_t main_processcommandline(int argc, char **argv) {
	int c;

	while ((c = getopt(argc, argv, "hvfc:")) != -1) {
		switch (c) {
			case '?': // Unknown option
			case 'h':
				main_printversion();
				printf("available arguments: -h        - this help\n");
				printf("                     -f        - run in foreground\n");
				printf("                     -c [file] - load config from file\n");
				return 0;
			case 'v':
				main_printversion();
				return 0;
			case 'f':
				main_flags.run_in_foreground = 1;
				break;
			case 'c':
				main_configfile = optarg;
				break;
			default:
				exit(1);
		}
	}
	return 1;
}

flag_t main_select(void) {
	server_sock_received_packet_t received_packet;
	fd_set rfds;
	struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 }; // Blocking only for 1 second.
	socklen_t addr_len;
	int api_client_sock_fd;
	int maxfd;

	FD_ZERO(&rfds);
	FD_SET(main_server_sock_fd, &rfds);
	maxfd = main_server_sock_fd;
	if (main_api_sock_fd >= 0) {
		FD_SET(main_api_sock_fd, &rfds);
		maxfd = max(maxfd, main_api_sock_fd);
		maxfd = max(maxfd, api_add_clients_to_fd_set(&rfds));
	}

	switch (select(maxfd+1, &rfds, NULL, NULL, &timeout)) {
		case -1:
			syslog(LOG_ERR, "main: select() error\n");
			main_flags.sigexit = 1;
			return 0;
		case 0: // Timeout
			return 1;
		default:
			if (FD_ISSET(main_server_sock_fd, &rfds)) {
				addr_len = sizeof(received_packet.from_addr);
				received_packet.received_bytes = recvfrom(main_server_sock_fd, received_packet.buf,
						sizeof(received_packet.buf), 0, &received_packet.from_addr, &addr_len);

				if (received_packet.received_bytes >= sizeof(srf_ip_conn_packet_header_t) &&
					srf_ip_conn_packet_is_header_valid((srf_ip_conn_packet_header_t *)received_packet.buf))
				{
					packet_process(&received_packet);
				}
			}

			if (FD_ISSET(main_api_sock_fd, &rfds)) {
				addr_len = sizeof(received_packet.from_addr);
				if ((api_client_sock_fd = accept(main_api_sock_fd, (struct sockaddr *)&received_packet.from_addr, &addr_len)) >= 0) {
					//syslog(LOG_INFO, "main: new api client %u\n", api_client_sock_fd);
					api_client_add(api_client_sock_fd);
				}
			}

			api_process_fd_set(&rfds);
			return 1;
	}
}

int main(int argc, char **argv) {
	if (!main_processcommandline(argc, argv))
		return 0;

	if (main_flags.run_in_foreground)
		openlog(NULL, LOG_PERROR | LOG_CONS, LOG_DAEMON);
	else
		openlog(NULL, LOG_PID, LOG_DAEMON);

	if (!config_read(main_configfile)) {
		closelog();
		return 0;
	}

	if (!main_flags.run_in_foreground) {
		switch (main_daemonize()) {
			default:
				closelog();
				return 1;
			case MAIN_FORK_RESULT_OK_PARENTEXIT:
				closelog();
				return 0;
			case MAIN_FORK_RESULT_OK:
				break;
		}
	}

	main_writepidfile();

	srand(time(NULL));
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, main_sighandler);
	signal(SIGTERM, main_sighandler);
	signal(SIGPIPE, SIG_IGN);

	if ((main_server_sock_fd = server_sock_init(config_port, config_ipv4_only, config_bind_ip_str)) < 0) {
		closelog();
		return 0;
	}
	main_api_sock_fd = api_init(config_api_socket_file_str);

	syslog(LOG_INFO, "main: starting loop\n");
	main_started_at = time(NULL);

	while (!main_flags.sigexit) {
		main_select();

		client_process();
		api_process();
	}

	close(main_api_sock_fd);
	unlink(config_api_socket_file_str);
	close(main_server_sock_fd);
	unlink(config_pidfile_str);
	closelog();

	return 0;
}
