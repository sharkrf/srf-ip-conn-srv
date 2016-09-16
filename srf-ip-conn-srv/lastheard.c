#include "lastheard.h"
#include "config.h"
#include "client.h"

#include <stdlib.h>

typedef struct lastheard_entry {
	uint32_t id;
	time_t at;
	lastheard_mode_t mode;
	time_t duration;
	uint32_t call_session_id;

	struct lastheard_entry *next;
	struct lastheard_entry *prev;
} lastheard_entry_t;

static lastheard_entry_t *lastheards = NULL;
static int lastheards_count = 0;

void lastheard_add(uint32_t client_id, uint32_t call_session_id, lastheard_mode_t mode, time_t duration) {
	lastheard_entry_t *newentry;

	// If the first entry matches the client and call session ids, we only update it's timestamp.
	if (lastheards != NULL && lastheards->id == client_id && lastheards->call_session_id == call_session_id) {
		printf("update %.8x\n", call_session_id);
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

	printf("add %.8x\n", call_session_id);
	newentry->id = client_id;
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
	int res_size = 48+lastheards_count*100;
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

		printed_chars = snprintf(res+resp, res_remaining_size, "{\"id\":%u,\"at\":%lu,\"mode\":%u,\"duration\":%lu}",
				lh->id, lh->at, lh->mode, lh->duration);
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
