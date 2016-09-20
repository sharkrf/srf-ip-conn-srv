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

#include "banlist.h"
#include "json.h"
#include "sock.h"

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#define BANLIST_MAX_ITEM_COUNT		1000

typedef struct banlist_client_id_entry {
	uint32_t id;

	struct banlist_client_id_entry *next;
} banlist_client_id_entry_t;

typedef struct banlist_client_ip_entry {
	struct sockaddr ip;

	struct banlist_client_ip_entry *next;
} banlist_client_ip_entry_t;

static banlist_client_id_entry_t *banlist_client_ids = NULL;
static banlist_client_ip_entry_t *banlist_client_ips = NULL;
static int banlist_entry_count = 0;

static void banlist_clear(void) {
	banlist_client_id_entry_t *cid = banlist_client_ids;
	banlist_client_id_entry_t *cid_prev;
	banlist_client_ip_entry_t *cip = banlist_client_ips;
	banlist_client_ip_entry_t *cip_prev;

	while (cid) {
		cid_prev = cid;
		cid = cid->next;
		free(cid_prev);
	}
	banlist_client_ids = NULL;

	while (cip) {
		cip_prev = cip;
		cip = cip->next;
		free(cip_prev);
	}
	banlist_client_ips = NULL;
}

static void banlist_add_client_id(uint32_t client_id) {
	banlist_client_id_entry_t *newentry;

	newentry = (banlist_client_id_entry_t *)calloc(sizeof(banlist_client_id_entry_t), 1);
	if (newentry == NULL)
		return;

	syslog(LOG_INFO, "banlist: adding client id %u\n", client_id);

	newentry->id = client_id;

	if (banlist_client_ids == NULL)
		banlist_client_ids = newentry;
	else {
		// Inserting to the beginning of the list.
		newentry->next = banlist_client_ids;
		banlist_client_ids = newentry;
	}

	banlist_entry_count++;
}

static void banlist_add_client_ip(struct sockaddr *ip) {
	banlist_client_ip_entry_t *newentry;
	char s[INET6_ADDRSTRLEN];

	newentry = (banlist_client_ip_entry_t *)calloc(sizeof(banlist_client_ip_entry_t), 1);
	if (newentry == NULL)
		return;

	syslog(LOG_INFO, "banlist: adding client ip %s\n",
			inet_ntop(ip->sa_family, sock_get_in_addr(ip), s, sizeof(s)));

	memcpy(&newentry->ip, ip, sizeof(struct sockaddr));

	if (banlist_client_ips == NULL)
		banlist_client_ips = newentry;
	else {
		// Inserting to the beginning of the list.
		newentry->next = banlist_client_ips;
		banlist_client_ips = newentry;
	}

	banlist_entry_count++;
}

flag_t banlist_is_banned_client_id(uint32_t client_id) {
	banlist_client_id_entry_t *cid = banlist_client_ids;

	while (cid) {
		if (cid->id == client_id)
			return 1;
		cid = cid->next;
	}
	return 0;
}

flag_t banlist_is_banned_client_ip(struct sockaddr *addr) {
	banlist_client_ip_entry_t *cip = banlist_client_ips;

	while (cip) {
		if (sock_is_sockaddr_ip_match(addr, &cip->ip))
			return 1;
		cip = cip->next;
	}
	return 0;
}

/*char *banlist_build_list_json(void) {
	char *res;
	int res_size = 48+banlist_entry_count*100;
	int res_remaining_size = res_size;
	int resp;
	int printed_chars;
	banlist_client_id_entry_t *cids = banlist_client_ids;
	banlist_client_ip_entry_t *cips = banlist_client_ips;
	int i;
	char s[INET6_ADDRSTRLEN];

	res = (char *)calloc(res_size+1, 1); // +1 so response will be always null-terminated.
	if (res == NULL)
		return NULL;

	printed_chars = snprintf(res, res_size, "{\n\t\"client-ids\": [ ");
	res_remaining_size -= printed_chars;
	resp = printed_chars;
	i = 0;
	while (cids) {
		if (i++ > 0) {
			printed_chars = snprintf(res+resp, res_remaining_size, ", ");
			res_remaining_size -= printed_chars;
			if (res_remaining_size <= 0)
				break;
			resp += printed_chars;
		}

		printed_chars = snprintf(res+resp, res_remaining_size, "%u", cids->id);
		res_remaining_size -= printed_chars;
		if (res_remaining_size <= 0)
			break;
		resp += printed_chars;

		cids = cids->next;
	}
	printed_chars = snprintf(res+resp, res_remaining_size, " ],\n\t\"client-ips\": [ ");
	res_remaining_size -= printed_chars;
	if (res_remaining_size <= 0)
		break;
	resp += printed_chars;

	res_remaining_size -= printed_chars;
	resp = printed_chars;
	i = 0;
	while (cips) {
		if (i++ > 0) {
			printed_chars = snprintf(res+resp, res_remaining_size, ", ");
			res_remaining_size -= printed_chars;
			if (res_remaining_size <= 0)
				break;
			resp += printed_chars;
		}

		printed_chars = snprintf(res+resp, res_remaining_size, "%s",
				inet_ntop(cips->ip.sa_family, sock_get_in_addr(&cips->ip), s, sizeof(s)));
		res_remaining_size -= printed_chars;
		if (res_remaining_size <= 0)
			break;
		resp += printed_chars;

		cips = cips->next;
	}
	snprintf(res+resp, res_remaining_size, " ]\n}\n");

	return res;
}*/

void banlist_load(char *filename) {
	FILE *f;
	long fsize;
	char *buf;
	jsmn_parser json_parser;
	jsmntok_t tok[20+10*BANLIST_MAX_ITEM_COUNT];
	int json_entry_count;
	int i;
	char *endptr;
	char id_str[11] = {0,};
	uint32_t id;
	char ip_str[INET6_ADDRSTRLEN] = {0,};
	enum { PARSING_NONE, PARSING_IDS, PARSING_IPS } parsing = PARSING_NONE;
	union {
		struct sockaddr_in ip4;
		struct sockaddr_in6 ip6;
	} ip_addr;

	f = fopen(filename, "r");
	if (f == NULL) {
		syslog(LOG_ERR, "banlist: can't open banlist file for reading: %s\n", filename);
		return;
	}

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	rewind(f);

	buf = (char *)malloc(fsize);
	if (buf == NULL) {
		fclose(f);
		syslog(LOG_ERR, "banlist: can't allocate memory for banlist file reading: %s\n", filename);
		return;
	}

	if (fread(buf, 1, fsize, f) != fsize) {
		fclose(f);
		free(buf);
		syslog(LOG_ERR, "banlist: error reading file: %s\n", filename);
	}
	fclose(f);

	jsmn_init(&json_parser);
	json_entry_count = jsmn_parse(&json_parser, buf, fsize, tok, sizeof(tok) / sizeof(tok[0]));
	if (json_entry_count < 1 || tok[0].type != JSMN_OBJECT) {
		free(buf);
		syslog(LOG_ERR, "banlist: error parsing file: %s\n", filename);
		return;
	}

	banlist_clear();

	for (i = 1; i < json_entry_count; i++) {
		if (json_compare_tok_key(buf, &tok[i], "client-ids")) {
			parsing = PARSING_IDS;
			i++;
		} else if (json_compare_tok_key(buf, &tok[i], "client-ips")) {
			parsing = PARSING_IPS;
			i++;
		} else if (parsing == PARSING_IDS && tok[i].type == JSMN_PRIMITIVE) {
			json_get_value(buf, &tok[i], id_str, sizeof(id_str));
			errno = 0;
			id = strtoll(id_str, &endptr, 10);
			if (errno == 0 && *endptr == 0)
				banlist_add_client_id(id);
		} else if (parsing == PARSING_IPS && tok[i].type == JSMN_STRING) {
			json_get_value(buf, &tok[i], ip_str, sizeof(ip_str));
			if (strchr(ip_str, ':')) {
				if (inet_pton(AF_INET6, ip_str, &ip_addr.ip6.sin6_addr) == 1) {
					ip_addr.ip6.sin6_family = AF_INET6;
					banlist_add_client_ip((struct sockaddr *)&ip_addr.ip6);
				}
			} else {
				if (inet_pton(AF_INET, ip_str, &ip_addr.ip4.sin_addr) == 1) {
					ip_addr.ip4.sin_family = AF_INET;
					banlist_add_client_ip((struct sockaddr *)&ip_addr.ip4);
				}
			}
		} else {
			free(buf);
			syslog(LOG_ERR, "banlist: unexpected key at %u\n", tok[i].start);
			return;
		}
	}

	free(buf);
}
