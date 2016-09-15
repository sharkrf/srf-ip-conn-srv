#include "lastheard.h"
#include "config.h"

#include <time.h>
#include <stdlib.h>

typedef struct lastheard_entry {
	uint32_t id;
	time_t at;
	lastheard_mode_t mode;

	struct lastheard_entry *next;
	struct lastheard_entry *prev;
} lastheard_entry_t;

static lastheard_entry_t *lastheards = NULL;
static int lastheards_count = 0;

void lastheard_add(uint32_t id, lastheard_mode_t mode) {
	lastheard_entry_t *newentry;

	// If the first entry matches the id, we only update it's timestamp.
	if (lastheards != NULL && lastheards->id == id) {
		lastheards->at = time(NULL);
		lastheards->mode = mode;
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

	newentry->id = id;
	newentry->at = time(NULL);
	newentry->mode = mode;

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
	int res_size = 36+lastheards_count*100;
	int res_remaining_size = res_size;
	int resp;
	int printed_chars;
	lastheard_entry_t *lh = lastheards;
	int i = 0;

	res = (char *)calloc(res_size+1, 1); // +1 so response will be always null-terminated.
	if (res == NULL)
		return NULL;

	printed_chars = snprintf(res, res_size, "{\"req\":\"lastheard-list\",\"list\":[");
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

		printed_chars = snprintf(res+resp, res_remaining_size, "{\"id\":%u,\"at\":%lu,\"mode\":%u}",
				lh->id, lh->at, lh->mode);
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
