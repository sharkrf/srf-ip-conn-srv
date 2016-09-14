#include "json.h"

#include <string.h>

// Returns 1 if given *tok equals *key in the given *request.
flag_t json_compare_tok_key(const char *request, jsmntok_t *tok, const char *key) {
	return (tok->type == JSMN_STRING &&
			(int)strlen(key) == tok->end - tok->start &&
			strncmp(request + tok->start, key, tok->end - tok->start) == 0);
}

void json_get_value(const char *request, jsmntok_t *tok, char *value, size_t value_size) {
	memcpy(value, request+tok->start, min(value_size-1, tok->end-tok->start));
	value[value_size-1] = 0;
}
