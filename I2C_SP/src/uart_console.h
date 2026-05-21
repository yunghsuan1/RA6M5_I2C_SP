#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include "hal_data.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

// LED 控制腳位與狀態定義
typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK
} led_state_t;

#define LED_BLUE_PIN   BSP_IO_PORT_00_PIN_06
#define LED_GREEN_PIN  BSP_IO_PORT_00_PIN_07
#define LED_RED_PIN    BSP_IO_PORT_00_PIN_08

// 序列埠控制台結構體定義
typedef struct {
    uart_ctrl_t *p_ctrl;
    uart_cfg_t const *p_cfg;
    ring_buffer_t tx_ring;
    ring_buffer_t rx_ring;
    uint8_t tx_buf_data[512]; // 發送緩衝區
    uint8_t rx_buf_data[128]; // 接收緩衝區
    volatile bool tx_busy;
    uint8_t tx_active_char;   // 保存當前正在發送的字元，確保位址穩定
    uint8_t rx_char_temp;
    char cmd_line[128];       // 指令重組暫存區
    uint8_t cmd_idx;
} uart_console_t;

// 外部宣告兩個控制台實例
extern uart_console_t g_console_master;
extern uart_console_t g_console_slave;

// 控制台 API
void uart_console_init(void);
void uart_console_print(uart_console_t *p_con, const char *format, ...);
void uart_console_process_rx(uart_console_t *p_con, const char *console_name);
void uart_console_isr_handler(uart_console_t *p_con, uart_callback_args_t *p_args);

// LED 控制 API
void led_init(void);
void led_process(void);
void led_set_state(uint8_t color, led_state_t state);
void led_get_all_states(uint8_t *p_blue, uint8_t *p_green, uint8_t *p_red);

#ifdef __cplusplus
}
#endif

#endif // UART_CONSOLE_H
