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
    time_t last_data_packet_at;
    uint32_t rx_seqnum;
    uint32_t tx_seqnum;

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
extern client_t *client_in_call;
extern time_t client_in_call_started_at;

char *client_build_config_json(uint32_t client_id);
char *client_build_list_json(void);

ignored_ip_t *client_ignored_ip_search(struct sockaddr *addr);
void client_ignored_ip_add(struct sockaddr *addr);

client_t *client_login_search(struct sockaddr *from_addr);
client_t *client_login_add(uint32_t client_id, struct sockaddr *from_addr);
void client_login_delete(client_t *client, flag_t free_allocated_memory);

client_t *client_search(struct sockaddr *from_addr);
void client_add(client_t *client);
void client_delete(client_t *client);
void client_broadcast(client_t *from_client, srf_ip_conn_packet_t *packet, uint16_t data_len,
		uint16_t payload_len, uint32_t missing_packets_num);

void client_process(void);

#endif
