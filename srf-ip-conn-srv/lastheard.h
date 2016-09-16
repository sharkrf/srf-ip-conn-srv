#ifndef LASTHEARD_H_
#define LASTHEARD_H_

#include "types.h"

#include <time.h>

#define LASTHEARD_MODE_RAW		0
#define LASTHEARD_MODE_DMR		1
#define LASTHEARD_MODE_DSTAR	2
#define LASTHEARD_MODE_C4FM		3
typedef uint8_t lastheard_mode_t;

void lastheard_add(uint32_t client_id, uint32_t call_session_id, lastheard_mode_t mode, time_t duration);
char *lastheard_build_list_json(void);

#endif
