#ifndef MURMUR_HASH_H
#define MURMUR_HASH_H

#include <stdint.h> 

uint32_t murmur_hash(const void *key, int len, uint32_t seed);

#endif
