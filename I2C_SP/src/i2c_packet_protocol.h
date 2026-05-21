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
