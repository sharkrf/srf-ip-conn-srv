#include "types.h"
#include "config.h"
#include "packet.h"
#include "client.h"

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

int main(int argc, char **argv) {
	server_sock_received_packet_t received_packet;

	if (!main_processcommandline(argc, argv))
		return 0;

	if (main_flags.run_in_foreground)
		openlog(NULL, LOG_PERROR | LOG_CONS, LOG_DAEMON);
	else
		openlog(NULL, 0, LOG_DAEMON);

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

	if (!server_sock_init(config_port, config_ipv4_only, config_bind_ip_str)) {
		closelog();
		return 0;
	}

	syslog(LOG_INFO, "main: starting loop\n");

	while (!main_flags.sigexit) {
		if (server_sock_receive(&received_packet) < 0)
			break;

		if (received_packet.received_bytes >= sizeof(srf_ip_conn_packet_header_t) &&
			srf_ip_conn_packet_is_header_valid((srf_ip_conn_packet_header_t *)received_packet.buf))
		{
			packet_process(&received_packet);
			received_packet.received_bytes = 0;
		}

		client_process();
	}

	server_sock_deinit();
	unlink(config_pidfile_str);
	closelog();

	return 0;
}
