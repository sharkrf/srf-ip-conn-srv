#ifndef CLIENT_H_
#define CLIENT_H_

#include "types.h"

#include <srf-ip-conn/common/srf-ip-conn-packet.h>

#include <time.h>
#include <netinet/in.h>

typedef struct clients {
    uint32_t client_id;
    uint8_t token[SRF_IP_CONN_TOKEN_LENGTH];
    struct sockaddr from_addr;
    time_t last_valid_packet_got_at;

    flag_t got_config;
    srf_ip_conn_config_payload_t config;

  	struct clients *next;
   	struct clients *prev;
} client_t;

typedef struct ignored_ip {
	struct sockaddr addr;
	time_t added_at;

	struct ignored_ip *next;
	struct ignored_ip *prev;
} ignored_ip_t;

extern uint16_t clients_login_count;
extern uint16_t clients_count;

ignored_ip_t *client_ignored_ip_search(struct sockaddr *addr);
void client_ignored_ip_add(struct sockaddr *addr);

client_t *client_login_search(struct sockaddr *from_addr);
client_t *client_login_add(uint32_t client_id, struct sockaddr *from_addr);
void client_login_delete(client_t *client, flag_t free_allocated_memory);

client_t *client_search(struct sockaddr *from_addr);
void client_add(client_t *client);
void client_delete(client_t *client);
void client_broadcast(client_t *from_client, srf_ip_conn_packet_t *packet, uint16_t data_len, uint16_t payload_len);

void client_process(void);

#endif
