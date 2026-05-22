#include "uart_console.h"
#include "i2c_master.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

// 定義系統 Tick
volatile uint32_t g_system_ticks = 0;

// 全域 LED 狀態
static led_state_t g_led_blue_state = LED_STATE_OFF;
static led_state_t g_led_green_state = LED_STATE_OFF;
static led_state_t g_led_red_state = LED_STATE_OFF;

// 實例化兩個控制台結構體
uart_console_t g_console_master;
uart_console_t g_console_slave;

// 輔助函式：將字串轉為小寫並修剪首尾空白
static void trim_and_lowercase(char *str) {
    int len = (int)strlen(str);
    // 修剪右側空白
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }
    // 修剪左側空白
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    // 轉為小寫
    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

// 初始化控制台
void uart_console_init(void) {
    // 1. 初始化 Master Console (UART9)
    g_console_master.p_ctrl = &g_uart9_ctrl;
    g_console_master.p_cfg = &g_uart9_cfg;
    g_console_master.tx_busy = false;
    g_console_master.cmd_idx = 0;
    g_console_master.tx_active_char = 0;
    memset(g_console_master.cmd_line, 0, sizeof(g_console_master.cmd_line));
    ring_buffer_init(&g_console_master.tx_ring, g_console_master.tx_buf_data, sizeof(g_console_master.tx_buf_data));
    ring_buffer_init(&g_console_master.rx_ring, g_console_master.rx_buf_data, sizeof(g_console_master.rx_buf_data));

    // 2. 初始化 Slave Console (UART8)
    g_console_slave.p_ctrl = &g_uart8_ctrl;
    g_console_slave.p_cfg = &g_uart8_cfg;
    g_console_slave.tx_busy = false;
    g_console_slave.cmd_idx = 0;
    g_console_slave.tx_active_char = 0;
    memset(g_console_slave.cmd_line, 0, sizeof(g_console_slave.cmd_line));
    ring_buffer_init(&g_console_slave.tx_ring, g_console_slave.tx_buf_data, sizeof(g_console_slave.tx_buf_data));
    ring_buffer_init(&g_console_slave.rx_ring, g_console_slave.rx_buf_data, sizeof(g_console_slave.rx_buf_data));

    // 3. 開啟 FSP UART 驅動
    R_SCI_UART_Open(g_console_master.p_ctrl, g_console_master.p_cfg);
    R_SCI_UART_Open(g_console_slave.p_ctrl, g_console_slave.p_cfg);
}

// 非阻塞列印（格式化字串送入 ring buffer 並啟動發送）
void uart_console_print(uart_console_t *p_con, const char *format, ...) {
    char buf[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len <= 0) {
        return;
    }

    // 關閉中斷保護臨界區
    __disable_irq();

    // 寫入 TX 環形緩衝區
    for (int i = 0; i < len; i++) {
        ring_buffer_push(&p_con->tx_ring, (uint8_t)buf[i]);
    }

    // 若硬體發送處於空閒，則提取首個位元組發送
    if (!p_con->tx_busy) {
        if (ring_buffer_pop(&p_con->tx_ring, (uint8_t *)&p_con->tx_active_char)) {
            p_con->tx_busy = true;
            R_SCI_UART_Write(p_con->p_ctrl, (uint8_t *)&p_con->tx_active_char, 1);
        }
    }

    __enable_irq();
}

// 處理中斷回呼 (將此函式置於 callback 中)
void uart_console_isr_handler(uart_console_t *p_con, uart_callback_args_t *p_args) {
    switch (p_args->event) {
        case UART_EVENT_RX_CHAR: {
            // 收到字元 (不使用 Read 模式，由 FSP 自動觸發非同步 RX_CHAR)，推入接收環形緩衝區
            ring_buffer_push(&p_con->rx_ring, (uint8_t)p_args->data);
            break;
        }
        case UART_EVENT_TX_COMPLETE: {
            // 前一字元發送完成，嘗試發送下一個
            if (ring_buffer_pop(&p_con->tx_ring, (uint8_t *)&p_con->tx_active_char)) {
                R_SCI_UART_Write(p_con->p_ctrl, (uint8_t *)&p_con->tx_active_char, 1);
            } else {
                p_con->tx_busy = false; // 無資料，發送結束
            }
            break;
        }
        default:
            break;
    }
}

// FSP 圖形介面產生的 UART Callback 實體
void cb_g_uart9(uart_callback_args_t *p_args) {
    uart_console_isr_handler(&g_console_master, p_args);
}

void cb_g_uart8(uart_callback_args_t *p_args) {
    uart_console_isr_handler(&g_console_slave, p_args);
}

// 處理接收到的指令 (主迴圈輪詢)
void uart_console_process_rx(uart_console_t *p_con, const char *console_name) {
    uint8_t c;
    while (ring_buffer_pop(&p_con->rx_ring, &c)) {
        if (c == '\r' || c == '\n') {
            if (p_con->cmd_idx > 0) {
                p_con->cmd_line[p_con->cmd_idx] = '\0';
                
                // 複製指令用於解析
                char cmd[128];
                strcpy(cmd, p_con->cmd_line);
                trim_and_lowercase(cmd);

                uart_console_print(p_con, "\r\n[%s] 執行指令: %s\r\n", console_name, cmd);

                // 解析指令
                if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
                    uart_console_print(p_con, "====== 支援指令列表 ======\r\n");
                    uart_console_print(p_con, "  blue [on|off|blink]  - 控制藍燈(P006)\r\n");
                    uart_console_print(p_con, "  green [on|off|blink] - 控制綠燈(P007)\r\n");
                    uart_console_print(p_con, "  red [on|off|blink]   - 控制紅燈(P008)\r\n");
                    uart_console_print(p_con, "  status               - 查詢狀態 (LED / TSN 溫度)\r\n");
                    if (strcmp(console_name, "MASTER") == 0) {
                        uart_console_print(p_con, "  tsn                  - 讀取 MCU 真實內部溫度 (TSN)\r\n");
                        uart_console_print(p_con, "  scan                 - 掃描 I2C 總線裝置 (0x08 ~ 0x77)\r\n");
                    }
                    uart_console_print(p_con, "==========================\r\n");
                } 
                else if (strcmp(cmd, "status") == 0) {
                    const char *st_str[] = {"OFF", "ON", "BLINK"};
                    if (strcmp(console_name, "MASTER") == 0) {
                        uint8_t blue = 0, green = 0, red = 0;
                        float temp = 0.0f;
                        
                        uart_console_print(p_con, "[Master UART] 正在向 Slave 1 讀取 LED 狀態...\r\n");
                        fsp_err_t err1 = i2c_master_read_status(&blue, &green, &red);
                        if (FSP_SUCCESS == err1) {
                            uart_console_print(p_con, "  LED Status (來自 Slave 1): Blue=%s, Green=%s, Red=%s\r\n",
                                               st_str[blue], st_str[green], st_str[red]);
                        } else {
                            uart_console_print(p_con, "  [錯誤] 無法從 Slave 1 獲取 LED 狀態 (0x%X)\r\n", err1);
                        }

                        uart_console_print(p_con, "[Master UART] 正在向 Slave 2 讀取 TSN 溫度...\r\n");
                        fsp_err_t err2 = i2c_master_read_tsn(&temp);
                        if (FSP_SUCCESS == err2) {
                            uart_console_print(p_con, "  TSN Temperature (來自 Slave 2): %.2f °C\r\n", temp);
                        } else {
                            uart_console_print(p_con, "  [錯誤] 無法從 Slave 2 獲取 TSN 溫度 (0x%X)\r\n", err2);
                        }
                    } else {
                        uart_console_print(p_con, "LED Status: Blue=%s, Green=%s, Red=%s\r\n",
                                           st_str[g_led_blue_state], 
                                           st_str[g_led_green_state], 
                                           st_str[g_led_red_state]);
                    }
                }
                else if (strcmp(cmd, "tsn") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        float temp = 0.0f;
                        uart_console_print(p_con, "[Master UART] 正在透過 I2C 向 Slave 2 讀取 TSN 溫度...\r\n");
                        fsp_err_t err = i2c_master_read_tsn(&temp);
                        if (FSP_SUCCESS == err) {
                            uart_console_print(p_con, "[Master UART] 成功讀取 TSN 溫度: %.2f °C\r\n", temp);
                        } else {
                            uart_console_print(p_con, "[Master UART] 錯誤: 無法從 Slave 2 讀取 TSN 溫度 (0x%X)\r\n", err);
                        }
                    } else {
                        uart_console_print(p_con, "錯誤: tsn 指令僅限 Master 端執行。\r\n");
                    }
                }
                else if (strcmp(cmd, "scan") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        uart_console_print(p_con, "[SCAN] I2C bus scanning...\r\n");
                        uint8_t count = 0;
                        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
                            fsp_err_t err = i2c_master_probe(addr);
                            if (FSP_SUCCESS == err) {
                                uart_console_print(p_con, "[SCAN] Found device at 0x%02X\r\n", addr);
                                count++;
                            }
                        }
                        uart_console_print(p_con, "[SCAN] Total devices: %d\r\n", count);
                    } else {
                        uart_console_print(p_con, "錯誤: scan 指令僅限 Master 端執行。\r\n");
                    }
                }
                // 藍燈控制
                else if (strcmp(cmd, "blue on") == 0 || strcmp(cmd, "b1") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_BLUE, I2C_LED_MODE_ON);
                    } else {
                        led_set_state(I2C_LED_BLUE, LED_STATE_ON);
                        uart_console_print(p_con, "Blue LED: ON\r\n");
                    }
                }
                else if (strcmp(cmd, "blue off") == 0 || strcmp(cmd, "b0") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_BLUE, I2C_LED_MODE_OFF);
                    } else {
                        led_set_state(I2C_LED_BLUE, LED_STATE_OFF);
                        uart_console_print(p_con, "Blue LED: OFF\r\n");
                    }
                }
                else if (strcmp(cmd, "blue blink") == 0 || strcmp(cmd, "bb") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_BLUE, I2C_LED_MODE_BLINK);
                    } else {
                        led_set_state(I2C_LED_BLUE, LED_STATE_BLINK);
                        uart_console_print(p_con, "Blue LED: BLINK\r\n");
                    }
                }
                // 綠燈控制
                else if (strcmp(cmd, "green on") == 0 || strcmp(cmd, "g1") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_GREEN, I2C_LED_MODE_ON);
                    } else {
                        led_set_state(I2C_LED_GREEN, LED_STATE_ON);
                        uart_console_print(p_con, "Green LED: ON\r\n");
                    }
                }
                else if (strcmp(cmd, "green off") == 0 || strcmp(cmd, "g0") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_GREEN, I2C_LED_MODE_OFF);
                    } else {
                        led_set_state(I2C_LED_GREEN, LED_STATE_OFF);
                        uart_console_print(p_con, "Green LED: OFF\r\n");
                    }
                }
                else if (strcmp(cmd, "green blink") == 0 || strcmp(cmd, "gb") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_GREEN, I2C_LED_MODE_BLINK);
                    } else {
                        led_set_state(I2C_LED_GREEN, LED_STATE_BLINK);
                        uart_console_print(p_con, "Green LED: BLINK\r\n");
                    }
                }
                // 紅燈控制
                else if (strcmp(cmd, "red on") == 0 || strcmp(cmd, "r1") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_RED, I2C_LED_MODE_ON);
                    } else {
                        led_set_state(I2C_LED_RED, LED_STATE_ON);
                        uart_console_print(p_con, "Red LED: ON\r\n");
                    }
                }
                else if (strcmp(cmd, "red off") == 0 || strcmp(cmd, "r0") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_RED, I2C_LED_MODE_OFF);
                    } else {
                        led_set_state(I2C_LED_RED, LED_STATE_OFF);
                        uart_console_print(p_con, "Red LED: OFF\r\n");
                    }
                }
                else if (strcmp(cmd, "red blink") == 0 || strcmp(cmd, "rb") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        i2c_master_send_led_cmd(I2C_LED_RED, I2C_LED_MODE_BLINK);
                    } else {
                        led_set_state(I2C_LED_RED, LED_STATE_BLINK);
                        uart_console_print(p_con, "Red LED: BLINK\r\n");
                    }
                }
                else {
                    uart_console_print(p_con, "錯誤: 未知指令 '%s'。輸入 'help' 查詢指令。\r\n", cmd);
                }

                // 重新清空緩衝區
                p_con->cmd_idx = 0;
            }
        } 
        else if (c >= 32 && c <= 126) {
            // 可列印字元，存入緩衝區並在終端機回顯 (Echo)
            if (p_con->cmd_idx < sizeof(p_con->cmd_line) - 1) {
                p_con->cmd_line[p_con->cmd_idx++] = (char)c;
                // Echo 回顯字元 (推至 TX 環形緩衝區非阻塞發送，避免 busy 衝突)
                uart_console_print(p_con, "%c", c);
            }
        }
    }
}

// 初始化 LED
void led_init(void) {
    g_ioport.p_api->pinWrite(&g_ioport_ctrl, LED_BLUE_PIN, BSP_IO_LEVEL_LOW);
    g_ioport.p_api->pinWrite(&g_ioport_ctrl, LED_GREEN_PIN, BSP_IO_LEVEL_LOW);
    g_ioport.p_api->pinWrite(&g_ioport_ctrl, LED_RED_PIN, BSP_IO_LEVEL_LOW);
}

// 處理 LED 閃爍狀態 (每 250ms 翻轉一次狀態)
void led_process(void) {
    static uint32_t last_toggle_tick = 0;
    static bsp_io_level_t blink_level = BSP_IO_LEVEL_LOW;

    if (g_system_ticks - last_toggle_tick >= 250) {
        last_toggle_tick = g_system_ticks;
        blink_level = (blink_level == BSP_IO_LEVEL_LOW) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;

        // 藍燈閃爍
        if (g_led_blue_state == LED_STATE_BLINK) {
            g_ioport.p_api->pinWrite(&g_ioport_ctrl, LED_BLUE_PIN, blink_level);
        }
        // 綠燈閃爍
        if (g_led_green_state == LED_STATE_BLINK) {
            g_ioport.p_api->pinWrite(&g_ioport_ctrl, LED_GREEN_PIN, blink_level);
        }
        // 紅燈閃爍
        if (g_led_red_state == LED_STATE_BLINK) {
            g_ioport.p_api->pinWrite(&g_ioport_ctrl, LED_RED_PIN, blink_level);
        }
    }
}

// 供 I2C Slave 控制 LED 狀態與腳位
void led_set_state(uint8_t color, led_state_t state) {
    bsp_io_port_pin_t pin;
    if (color == 0) pin = LED_BLUE_PIN;
    else if (color == 1) pin = LED_GREEN_PIN;
    else if (color == 2) pin = LED_RED_PIN;
    else return;

    if (color == 0) g_led_blue_state = state;
    else if (color == 1) g_led_green_state = state;
    else if (color == 2) g_led_red_state = state;

    if (state == LED_STATE_ON) {
        g_ioport.p_api->pinWrite(&g_ioport_ctrl, pin, BSP_IO_LEVEL_HIGH);
    } else if (state == LED_STATE_OFF) {
        g_ioport.p_api->pinWrite(&g_ioport_ctrl, pin, BSP_IO_LEVEL_LOW);
    }
}

// 取得所有 LED 狀態的目前值
void led_get_all_states(uint8_t *p_blue, uint8_t *p_green, uint8_t *p_red) {
    *p_blue = (uint8_t)g_led_blue_state;
    *p_green = (uint8_t)g_led_green_state;
    *p_red = (uint8_t)g_led_red_state;
}
