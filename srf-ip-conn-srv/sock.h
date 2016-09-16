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

#ifndef SOCK_H_
#define SOCK_H_

#include "types.h"

#include <arpa/inet.h>

#define SOCK_ADDR_IN_PTR(sa)		((struct sockaddr_in *)(sa))
#define SOCK_ADDR_IN_FAMILY(sa)		SOCK_ADDR_IN_PTR(sa)->sin_family
#define SOCK_ADDR_IN_PORT(sa)		SOCK_ADDR_IN_PTR(sa)->sin_port
#define SOCK_ADDR_IN_ADDR(sa)		SOCK_ADDR_IN_PTR(sa)->sin_addr

#define SOCK_ADDR_IN6_PTR(sa)		((struct sockaddr_in6 *)(sa))
#define SOCK_ADDR_IN6_FAMILY(sa)	SOCK_ADDR_IN6_PTR(sa)->sin6_family
#define SOCK_ADDR_IN6_PORT(sa)		SOCK_ADDR_IN6_PTR(sa)->sin6_port
#define SOCK_ADDR_IN6_ADDR(sa)		SOCK_ADDR_IN6_PTR(sa)->sin6_addr

void *sock_get_in_addr(struct sockaddr *sa);
uint16_t sock_get_port(struct sockaddr *sa);
flag_t sock_is_sockaddr_ip_match(struct sockaddr *sa1, struct sockaddr *sa2);
flag_t sock_is_sockaddr_match(struct sockaddr *sa1, struct sockaddr *sa2);

#endif
