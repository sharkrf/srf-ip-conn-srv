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

#include "lastheard.h"
#include "config.h"
#include "client.h"

#include <stdlib.h>
#include <string.h>

typedef struct lastheard_entry {
	char to[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1];
	char from[SRF_IP_CONN_MAX_CALLSIGN_LENGTH+1];
	flag_t is_group_call;
	uint32_t client_id;
	time_t at;
	lastheard_mode_t mode;
	time_t duration;
	uint32_t call_session_id;

	struct lastheard_entry *next;
	struct lastheard_entry *prev;
} lastheard_entry_t;

static lastheard_entry_t *lastheards = NULL;
static int lastheards_count = 0;

void lastheard_add(char *to, char *from, flag_t is_group_call, uint32_t client_id, uint32_t call_session_id,
	lastheard_mode_t mode, time_t duration)
{
	lastheard_entry_t *newentry;

	// If the first entry matches the client and call session ids, we only update it's timestamp.
	if (lastheards != NULL && lastheards->client_id == client_id && lastheards->call_session_id == call_session_id) {
		if (memcmp(to, "N/A", 4) != 0) {
			strncpy(lastheards->to, to, sizeof(lastheards->to));
			lastheards->to[sizeof(lastheards->to)-1] = 0;

			lastheards->is_group_call = is_group_call;
		}
		if (memcmp(from, "N/A", 4) != 0) {
			strncpy(lastheards->from, from, sizeof(lastheards->from));
			lastheards->from[sizeof(lastheards->from)-1] = 0;
		}

		lastheards->at = time(NULL);
		lastheards->mode = mode;
		lastheards->duration = duration;
		return;
	}

	if (lastheards_count >= config_max_lastheard_entry_count && lastheards) {
		// Deleting the last element.
		newentry = lastheards;
		while (newentry->next)
			newentry = newentry->next;
		newentry->prev->next = NULL;
		free(newentry);
		lastheards_count--;
	}

	newentry = (lastheard_entry_t *)calloc(sizeof(lastheard_entry_t), 1);

	strncpy(newentry->to, to, sizeof(newentry->to));
	newentry->to[sizeof(newentry->to)-1] = 0;
	strncpy(newentry->from, from, sizeof(newentry->from));
	newentry->from[sizeof(newentry->from)-1] = 0;
	newentry->is_group_call = is_group_call;

	newentry->client_id = client_id;
	newentry->call_session_id = call_session_id;
	newentry->at = time(NULL);
	newentry->mode = mode;
	newentry->duration = duration;

	if (lastheards == NULL)
		lastheards = newentry;
	else {
		// Inserting to the beginning of the lastheards list.
		lastheards->prev = newentry;
		newentry->next = lastheards;
		lastheards = newentry;
	}

	lastheards_count++;
}

char *lastheard_build_list_json(void) {
	char *res;
	int res_size = 48+lastheards_count*(143+SRF_IP_CONN_MAX_CALLSIGN_LENGTH*2);
	int res_remaining_size = res_size;
	int resp;
	int printed_chars;
	lastheard_entry_t *lh = lastheards;
	int i = 0;

	res = (char *)calloc(res_size+1, 1); // +1 so response will be always null-terminated.
	if (res == NULL)
		return NULL;

	printed_chars = snprintf(res, res_size, "{\"req\":\"lastheard-list\",\"in-call\":%u,\"list\":[",
			(client_in_call != NULL));
	res_remaining_size -= printed_chars;
	resp = printed_chars;
	while (lh) {
		if (i++ > 0) {
			printed_chars = snprintf(res+resp, res_remaining_size, ",");
			res_remaining_size -= printed_chars;
			if (res_remaining_size <= 0)
				break;
			resp += printed_chars;
		}

		printed_chars = snprintf(res+resp, res_remaining_size, "{\"to\":\"%s\",\"from\":\"%s\",\"is-group-call\":%u,\"client-id\":%u,\"at\":%lu,\"mode\":%u,\"duration\":%lu}",
				lh->to, lh->from, lh->is_group_call, lh->client_id, lh->at, lh->mode, lh->duration);
		res_remaining_size -= printed_chars;
		if (res_remaining_size <= 0)
			break;
		resp += printed_chars;

		lh = lh->next;
	}
	if (res_remaining_size > 0)
		snprintf(res+resp, res_remaining_size, "]}");

	return res;
}
