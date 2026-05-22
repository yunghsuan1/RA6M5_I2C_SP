#ifndef I2C_SLAVE2_H
#define I2C_SLAVE2_H

#include "hal_data.h"
#include "i2c_packet_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Slave 2 初始化 (包含 TSN 與 ADC0 初始化)
void i2c_slave2_init(void);

// 在主迴圈輪詢：處理 Slave 2 異步列印等邏輯
void i2c_slave2_process(void);

// 讀取真實溫度感測器 (TSN) 的溫度值
float read_tsn_temp(void);

// I2C Slave 2 中斷回呼函式宣告 (與 FSP 中配置一致)
void cb_g_i2c_slave2(i2c_slave_callback_args_t *p_args);

#ifdef __cplusplus
}
#endif

#endif // I2C_SLAVE2_H
