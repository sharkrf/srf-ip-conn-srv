#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stdio.h>

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

typedef uint8_t flag_t;

#endif
