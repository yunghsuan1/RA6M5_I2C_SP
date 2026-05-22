#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include "hal_data.h"
#include "i2c_packet_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Master 初始化
void i2c_master_init(void);

// 發送 LED 控制封包至 Slave
fsp_err_t i2c_master_send_led_cmd(uint8_t led_color, uint8_t led_mode);

// 向 Slave 讀取 LED 狀態
fsp_err_t i2c_master_read_status(uint8_t *p_blue, uint8_t *p_green, uint8_t *p_red);

// 向 Slave 2 讀取 TSN 遙測溫度
fsp_err_t i2c_master_read_tsn(float *p_temp);

// 探測特定 I2C 位址是否存在從機 (Bus Scanner)
fsp_err_t i2c_master_probe(uint8_t addr);

// I2C Master 中斷回呼函式宣告 (與 FSP 配置一致)
void cb_g_i2c_master0(i2c_master_callback_args_t *p_args);

#ifdef __cplusplus
}
#endif

#endif // I2C_MASTER_H
