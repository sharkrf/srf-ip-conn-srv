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
