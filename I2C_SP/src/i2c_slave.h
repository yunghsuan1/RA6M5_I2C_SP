#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include "hal_data.h"
#include "i2c_packet_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Slave 初始化
void i2c_slave_init(void);

// 在主迴圈輪詢：處理接收到的 I2C 封包與控制 LED
void i2c_slave_process(void);

// I2C Slave 中斷回呼函式宣告 (與 FSP 配置一致)
void cb_g_i2c_slave0(i2c_slave_callback_args_t *p_args);

// 虛擬暫存器對外宣告
extern uint8_t g_slave_registers[0x20];

#ifdef __cplusplus
}
#endif

#endif // I2C_SLAVE_H
