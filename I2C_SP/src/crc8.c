#include "crc8.h"

uint8_t crc8_calc(const uint8_t *p_data, uint32_t length) {
    uint8_t crc = 0x00; // 初始值 0x00
    for (uint32_t i = 0; i < length; i++) {
        crc ^= p_data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ 0x07); // 多項式 0x07 (CRC-8-CCITT)
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}
