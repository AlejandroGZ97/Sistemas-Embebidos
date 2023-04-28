/*Receptor*/
#include "crc32.h"

uint32_t calc_crc32(uint8_t *data, uint32_t size)
{
    uint32_t crc = CRC32_INITIAL_VALUE;
    uint32_t i;

    for (i = 0; i < size; i++)
    {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return ~crc;
}