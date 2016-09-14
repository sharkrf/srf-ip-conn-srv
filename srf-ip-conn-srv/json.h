#ifndef JSON_H_
#define JSON_H_

#include "types.h"

#include <jsmn.h>

flag_t json_compare_tok_key(const char *request, jsmntok_t *tok, const char *key);
void json_get_value(const char *request, jsmntok_t *tok, char *value, size_t value_size);

#endif
