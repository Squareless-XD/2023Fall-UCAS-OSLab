#ifndef __INCLUDE_ENDIAN_H_
#define __INCLUDE_ENDIAN_H_

#include "type.h"

uint64_t b2l_endian_l(uint64_t);
uint32_t b2l_endian_w(uint32_t);
uint16_t b2l_endian_h(uint16_t);

uint64_t l2b_endian_l(uint64_t);
uint32_t l2b_endian_w(uint32_t);
uint16_t l2b_endian_h(uint16_t);

#endif // __INCLUDE_ENDIAN_H_
