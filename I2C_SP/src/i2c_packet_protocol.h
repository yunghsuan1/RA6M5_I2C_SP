#ifndef I2C_PACKET_PROTOCOL_H
#define I2C_PACKET_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 協定常數定義
#define I2C_PACKET_HEADER       0x5A     // 標記封包起始 (Header)
#define I2C_SLAVE_ADDR          0x4A     // Slave I2C 位址 (7-bit)
#define I2C_SLAVE2_ADDR         0x4B     // Slave 2 I2C 位址 (7-bit) - TSN 遙測

// 虛擬暫存器位址定義
#define REG_DEVICE_ID          0x00     // DEVICE_ID (唯讀, 預設 0xA5)
#define REG_FW_VERSION         0x01     // FW_VERSION (唯讀, 預設 0x01)
#define REG_BLUE_STATE         0x02     // BLUE_STATE (讀寫, 藍燈狀態)
#define REG_GREEN_STATE        0x03     // GREEN_STATE (讀寫, 綠燈狀態)
#define REG_RED_STATE          0x04     // RED_STATE (讀寫, 紅燈狀態)
#define REG_RX_PACKET_COUNT    0x10     // RX_PACKET_COUNT (唯讀, 成功接收封包數)
#define REG_CRC_FAIL_COUNT     0x11     // CRC_FAIL_COUNT (唯讀, CRC8 校驗失敗數)
#define REG_LAST_ERROR         0x12     // LAST_ERROR (唯讀, 上一次的錯誤代碼)
#define REG_LAST_COMMAND       0x13     // LAST_COMMAND (唯讀, 上一次執行的 Command ID)

// 暫存器 LAST_ERROR 錯誤代碼定義
#define REG_ERR_NONE           0x00     // 無錯誤
#define REG_ERR_BAD_CRC        0x01     // CRC8 錯誤
#define REG_ERR_BAD_HEADER     0x02     // Header 錯誤
#define REG_ERR_BAD_LEN        0x03     // 長度不符


// 指令集定義 (Command ID)
#define I2C_CMD_LED_CONTROL     0x01     // LED 控制指令

// LED 顏色編號
#define I2C_LED_BLUE            0x00     // 藍燈 (P006)
#define I2C_LED_GREEN           0x01     // 綠燈 (P007)
#define I2C_LED_RED             0x02     // 紅燈 (P008)

// LED 模式編號
#define I2C_LED_MODE_OFF        0x00     // 關閉
#define I2C_LED_MODE_ON         0x01     // 點亮
#define I2C_LED_MODE_BLINK      0x02     // 閃爍

// 封包結構體定義
#pragma pack(push, 1)
typedef struct {
    uint8_t header;     // 必須為 0x5A
    uint8_t length;     // Payload 的長度 (N)
    uint8_t command;    // 指令 ID
    uint8_t payload[64]; // 資料區 (最大暫定 64 位元組)
    uint8_t crc8;       // Header + Length + Command + Payload[0..N-1] 的 CRC8 校验值
} i2c_packet_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // I2C_PACKET_PROTOCOL_H
