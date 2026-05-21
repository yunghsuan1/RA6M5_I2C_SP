#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 計算資料的 CRC8 校验值 (多項式為 0x07, 初始值為 0x00)
 * 
 * @param p_data 指向資料緩衝區的指標
 * @param length 資料長度 (位元組)
 * @return uint8_t 計算出的 CRC8 值
 */
uint8_t crc8_calc(const uint8_t *p_data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif // CRC8_H
