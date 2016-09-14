#ifndef PACKET_H_
#define PACKET_H_

#include "server-sock.h"

void packet_process(server_sock_received_packet_t *received_packet);

#endif
