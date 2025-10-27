#ifndef CRC16_CCITT_H
#define CRC16_CCITT_H
#include <stdint.h>
#include <stddef.h>

uint16_t crc_ccitt_calc_first(const uint8_t *buffer, size_t len);
uint16_t crc_ccitt_calc_next(uint16_t crc, const uint8_t *buffer, size_t len);

#endif
