#include "uart_console.h"
#include "i2c_master.h"
#include "i2c_slave.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// 暫存器讀寫參數解析輔助函式
static bool parse_uint8(const char *str, uint8_t *val) {
    char *endptr;
    long lval = strtol(str, &endptr, 0);
    if (endptr == str) {
        return false;
    }
    if (lval < 0 || lval > 255) {
        return false;
    }
    *val = (uint8_t)lval;
    return true;
}

// 暫存器狀態人性化輸出輔助函式
static void print_register(uart_console_t *p_con, uint8_t reg, uint8_t val) {
    const char *err_str[] = {"NONE", "BAD_CRC", "BAD_HEADER", "BAD_LEN"};
    const char *state_str[] = {"OFF", "ON", "BLINK"};
    
    switch (reg) {
        case REG_DEVICE_ID:
            uart_console_print(p_con, "[REG] DEVICE_ID = 0x%02X\r\n", val);
            break;
        case REG_FW_VERSION:
            uart_console_print(p_con, "[REG] FW_VERSION = 0x%02X\r\n", val);
            break;
        case REG_BLUE_STATE:
            if (val <= 2) {
                uart_console_print(p_con, "[REG] BLUE_STATE = %d (%s)\r\n", val, state_str[val]);
            } else {
                uart_console_print(p_con, "[REG] BLUE_STATE = %d (INVALID)\r\n", val);
            }
            break;
        case REG_GREEN_STATE:
            if (val <= 2) {
                uart_console_print(p_con, "[REG] GREEN_STATE = %d (%s)\r\n", val, state_str[val]);
            } else {
                uart_console_print(p_con, "[REG] GREEN_STATE = %d (INVALID)\r\n", val);
            }
            break;
        case REG_RED_STATE:
            if (val <= 2) {
                uart_console_print(p_con, "[REG] RED_STATE = %d (%s)\r\n", val, state_str[val]);
            } else {
                uart_console_print(p_con, "[REG] RED_STATE = %d (INVALID)\r\n", val);
            }
            break;
        case REG_RX_PACKET_COUNT:
            uart_console_print(p_con, "[REG] RX_PACKET_COUNT = %d\r\n", val);
            break;
        case REG_CRC_FAIL_COUNT:
            uart_console_print(p_con, "[REG] CRC_FAIL_COUNT = %d\r\n", val);
            break;
        case REG_LAST_ERROR:
            if (val <= 3) {
                uart_console_print(p_con, "[REG] LAST_ERROR = %s\r\n", err_str[val]);
            } else {
                uart_console_print(p_con, "[REG] LAST_ERROR = %d (UNKNOWN)\r\n", val);
            }
            break;
        case REG_LAST_COMMAND:
            uart_console_print(p_con, "[REG] LAST_COMMAND = 0x%02X\r\n", val);
            break;
        default:
            uart_console_print(p_con, "[REG] REG_0x%02X = 0x%02X\r\n", reg, val);
            break;
    }
}

// 暫存器圖譜樹狀圖列印
static void print_regmap_tree(uart_console_t *p_con, const uint8_t *vals) {
    const char *err_str[] = {"NONE", "BAD_CRC", "BAD_HEADER", "BAD_LEN"};
    const char *state_str[] = {"OFF", "ON", "BLINK"};
    
    uart_console_print(p_con, "\r\nSlave 1 (0x4A) Register Map Tree:\r\n");
    
    // System Registers
    uart_console_print(p_con, "├── System Registers\r\n");
    uart_console_print(p_con, "│   ├── [0x00] DEVICE_ID   (RO) = 0x%02X  [Device ID]\r\n", vals[REG_DEVICE_ID]);
    uart_console_print(p_con, "│   └── [0x01] FW_VERSION  (RO) = 0x%02X  [Firmware Version]\r\n", vals[REG_FW_VERSION]);
    
    // LED Registers
    uart_console_print(p_con, "├── LED Control Registers (RW)\r\n");
    for (int i = 0; i < 3; i++) {
        uint8_t reg = REG_BLUE_STATE + i;
        uint8_t val = vals[reg];
        const char *name = (i == 0) ? "BLUE_STATE " : (i == 1) ? "GREEN_STATE" : "RED_STATE  ";
        const char *color_name = (i == 0) ? "Blue" : (i == 1) ? "Green" : "Red";
        const char *connector = (i == 2) ? "└──" : "├──";
        
        if (val <= 2) {
            uart_console_print(p_con, "│   %s [%02X] %s (RW) = %d (%s)  [%s LED state]\r\n", 
                               connector, reg, name, val, state_str[val], color_name);
        } else {
            uart_console_print(p_con, "│   %s [%02X] %s (RW) = %d (INVALID)  [%s LED state]\r\n", 
                               connector, reg, name, val, color_name);
        }
    }
    
    // Diagnostics
    uart_console_print(p_con, "└── Diagnostics & Statistics\r\n");
    uart_console_print(p_con, "    ├── [0x10] RX_PACKETS  (RO) = %d  [Valid custom packets count]\r\n", vals[REG_RX_PACKET_COUNT]);
    uart_console_print(p_con, "    ├── [0x11] CRC_FAILS   (RO) = %d  [CRC8 verification failures]\r\n", vals[REG_CRC_FAIL_COUNT]);
    
    uint8_t err = vals[REG_LAST_ERROR];
    if (err <= 3) {
        uart_console_print(p_con, "    ├── [0x12] LAST_ERROR  (RO) = %s  [Last packet error state]\r\n", err_str[err]);
    } else {
        uart_console_print(p_con, "    ├── [0x12] LAST_ERROR  (RO) = %d  [Unknown error state]\r\n", err);
    }
    
    uart_console_print(p_con, "    └── [0x13] LAST_CMD    (RO) = 0x%02X  [Last executed Command ID]\r\n\r\n", vals[REG_LAST_COMMAND]);
}

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
                    uart_console_print(p_con, "  readreg <reg>        - 讀取特定暫存器值 (Hex/Dec)\r\n");
                    uart_console_print(p_con, "  writereg <reg> <val> - 寫入特定暫存器值\r\n");
                    uart_console_print(p_con, "  dumpreg              - 傾印所有暫存器狀態\r\n");
                    uart_console_print(p_con, "  regmap               - 顯示暫存器圖譜樹狀圖\r\n");
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
                // 暫存器讀取指令
                else if (strncmp(cmd, "readreg", 7) == 0) {
                    const char *p = cmd + 7;
                    while (*p && isspace((unsigned char)*p)) {
                        p++;
                    }
                    uint8_t reg = 0;
                    if (*p != '\0' && parse_uint8(p, &reg)) {
                        if (strcmp(console_name, "MASTER") == 0) {
                            uint8_t val = 0;
                            uart_console_print(p_con, "[Master UART] 正在透過 I2C 向 Slave 1 讀取暫存器 0x%02X...\r\n", reg);
                            fsp_err_t err = i2c_master_read_reg(reg, &val);
                            if (FSP_SUCCESS == err) {
                                print_register(p_con, reg, val);
                            } else {
                                uart_console_print(p_con, "[Master UART] 錯誤: 讀取暫存器失敗 (0x%X)\r\n", err);
                            }
                        } else {
                            // Slave 本地控制台
                            if (reg < 0x20) {
                                print_register(p_con, reg, g_slave_registers[reg]);
                            } else {
                                uart_console_print(p_con, "錯誤: 暫存器位址超出範圍\r\n");
                            }
                        }
                    } else {
                        uart_console_print(p_con, "用法: readreg <reg_addr>\r\n");
                    }
                }
                // 暫存器寫入指令
                else if (strncmp(cmd, "writereg", 8) == 0) {
                    const char *p = cmd + 8;
                    while (*p && isspace((unsigned char)*p)) {
                        p++;
                    }
                    uint8_t reg = 0;
                    if (*p != '\0' && parse_uint8(p, &reg)) {
                        // 找尋空格區隔
                        while (*p && !isspace((unsigned char)*p)) {
                            p++;
                        }
                        while (*p && isspace((unsigned char)*p)) {
                            p++;
                        }
                        uint8_t val = 0;
                        if (*p != '\0' && parse_uint8(p, &val)) {
                            if (strcmp(console_name, "MASTER") == 0) {
                                uart_console_print(p_con, "[Master UART -> I2C] 轉發暫存器寫入指令: Reg 0x%02X = 0x%02X\r\n", reg, val);
                                fsp_err_t err = i2c_master_write_reg(reg, val);
                                if (FSP_SUCCESS == err) {
                                    uart_console_print(p_con, "[REG] Write Reg 0x%02X = 0x%02X 成功\r\n", reg, val);
                                } else {
                                    uart_console_print(p_con, "[Master UART] 錯誤: 寫入暫存器失敗 (0x%X)\r\n", err);
                                }
                            } else {
                                // Slave 本地控制台
                                if (reg < 0x20) {
                                    if (reg >= REG_BLUE_STATE && reg <= REG_RED_STATE) {
                                        g_slave_registers[reg] = val;
                                        uint8_t led_color = reg - REG_BLUE_STATE;
                                        led_set_state(led_color, (led_state_t)val);
                                        uart_console_print(p_con, "[REG] Write Reg 0x%02X = 0x%02X 成功\r\n", reg, val);
                                    } else {
                                        uart_console_print(p_con, "警告: 嘗試寫入唯讀暫存器 Reg 0x%02X\r\n", reg);
                                    }
                                } else {
                                    uart_console_print(p_con, "錯誤: 暫存器位址超出範圍\r\n");
                                }
                            }
                        } else {
                            uart_console_print(p_con, "用法: writereg <reg_addr> <value>\r\n");
                        }
                    } else {
                        uart_console_print(p_con, "用法: writereg <reg_addr> <value>\r\n");
                    }
                }
                // 暫存器 Dump 指令
                else if (strcmp(cmd, "dumpreg") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        uart_console_print(p_con, "[Master UART] 正在透過 I2C dump Slave 1 暫存器...\r\n");
                        uint8_t reg_list[] = {
                            REG_DEVICE_ID, REG_FW_VERSION,
                            REG_BLUE_STATE, REG_GREEN_STATE, REG_RED_STATE,
                            REG_RX_PACKET_COUNT, REG_CRC_FAIL_COUNT,
                            REG_LAST_ERROR, REG_LAST_COMMAND
                        };
                        bool success = true;
                        for (size_t i = 0; i < sizeof(reg_list)/sizeof(reg_list[0]); i++) {
                            uint8_t val = 0;
                            fsp_err_t err = i2c_master_read_reg(reg_list[i], &val);
                            if (FSP_SUCCESS == err) {
                                print_register(p_con, reg_list[i], val);
                            } else {
                                uart_console_print(p_con, "  [錯誤] 讀取暫存器 0x%02X 失敗 (0x%X)\r\n", reg_list[i], err);
                                success = false;
                                break;
                            }
                            R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
                        }
                        if (success) {
                            uart_console_print(p_con, "[Master UART] Dumpreg 完成\r\n");
                        }
                    } else {
                        // Slave 本地控制台
                        uart_console_print(p_con, "[Slave UART] Dump 本地暫存器...\r\n");
                        print_register(p_con, REG_DEVICE_ID, g_slave_registers[REG_DEVICE_ID]);
                        print_register(p_con, REG_FW_VERSION, g_slave_registers[REG_FW_VERSION]);
                        print_register(p_con, REG_BLUE_STATE, g_slave_registers[REG_BLUE_STATE]);
                        print_register(p_con, REG_GREEN_STATE, g_slave_registers[REG_GREEN_STATE]);
                        print_register(p_con, REG_RED_STATE, g_slave_registers[REG_RED_STATE]);
                        print_register(p_con, REG_RX_PACKET_COUNT, g_slave_registers[REG_RX_PACKET_COUNT]);
                        print_register(p_con, REG_CRC_FAIL_COUNT, g_slave_registers[REG_CRC_FAIL_COUNT]);
                        print_register(p_con, REG_LAST_ERROR, g_slave_registers[REG_LAST_ERROR]);
                        print_register(p_con, REG_LAST_COMMAND, g_slave_registers[REG_LAST_COMMAND]);
                    }
                }
                else if (strcmp(cmd, "regmap") == 0) {
                    if (strcmp(console_name, "MASTER") == 0) {
                        uart_console_print(p_con, "[Master UART] 正在透過 I2C 讀取 Slave 1 暫存器以生成樹狀圖...\r\n");
                        uint8_t reg_list[] = {
                            REG_DEVICE_ID, REG_FW_VERSION,
                            REG_BLUE_STATE, REG_GREEN_STATE, REG_RED_STATE,
                            REG_RX_PACKET_COUNT, REG_CRC_FAIL_COUNT,
                            REG_LAST_ERROR, REG_LAST_COMMAND
                        };
                        uint8_t vals[0x20] = {0};
                        bool success = true;
                        for (size_t i = 0; i < sizeof(reg_list)/sizeof(reg_list[0]); i++) {
                            uint8_t val = 0;
                            fsp_err_t err = i2c_master_read_reg(reg_list[i], &val);
                            if (FSP_SUCCESS == err) {
                                vals[reg_list[i]] = val;
                            } else {
                                uart_console_print(p_con, "  [錯誤] 讀取暫存器 0x%02X 失敗 (0x%X)\r\n", reg_list[i], err);
                                success = false;
                                break;
                            }
                            R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
                        }
                        if (success) {
                            print_regmap_tree(p_con, vals);
                        }
                    } else {
                        // Slave 本地控制台
                        print_regmap_tree(p_con, g_slave_registers);
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
