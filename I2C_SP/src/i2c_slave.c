#include "i2c_slave.h"
#include "crc8.h"
#include "uart_console.h"

// 全域接收變數
static uint8_t g_slave_rx_buf[128];
static volatile uint8_t g_slave_rx_idx = 0;
static volatile bool g_slave_packet_ready = false;

// 全域傳送變數
static volatile bool g_slave_tx_complete = false;
static uint8_t g_slave_tx_buf_copy[5];

// 暫存器映射與讀取狀態追蹤
uint8_t g_slave_registers[0x20] = {0};
static volatile uint8_t g_reg_pointer = 0;
static volatile bool g_reg_read_pending = false;
static volatile uint32_t g_reg_read_timestamp = 0;

// 初始化 I2C Slave 模組
void i2c_slave_init(void) {
    g_slave_rx_idx = 0;
    g_slave_packet_ready = false;
    g_slave_tx_complete = false;
    memset(g_slave_tx_buf_copy, 0, sizeof(g_slave_tx_buf_copy));
    
    // 初始化暫存器預設值
    memset(g_slave_registers, 0, sizeof(g_slave_registers));
    g_slave_registers[REG_DEVICE_ID] = 0xA5;
    g_slave_registers[REG_FW_VERSION] = 0x01;
    
    g_reg_pointer = 0;
    g_reg_read_pending = false;
    g_reg_read_timestamp = 0;

    // 開啟 FSP I2C Slave 驅動 (預期在 FSP 中命名為 g_i2c_slave0)
    R_IIC_SLAVE_Open(&g_i2c_slave0_ctrl, &g_i2c_slave0_cfg);
}

// 在主迴圈中輪詢處理接收到的封包
void i2c_slave_process(void) {
    if (g_slave_packet_ready) {
        // 保護臨界區避免與中斷衝突
        __disable_irq();
        uint8_t rx_len = g_slave_rx_idx;
        uint8_t packet_copy[128];
        memcpy(packet_copy, g_slave_rx_buf, rx_len);
        g_slave_packet_ready = false;
        __enable_irq();

        // 1. 處理 2 位元組的暫存器寫入 (非 0x5A 起始)
        if (rx_len == 2 && packet_copy[0] != I2C_PACKET_HEADER) {
            uint8_t reg_addr = packet_copy[0];
            uint8_t reg_val  = packet_copy[1];
            if (reg_addr < 0x20) {
                // 僅 LED 狀態暫存器允許寫入控制
                if (reg_addr >= REG_BLUE_STATE && reg_addr <= REG_RED_STATE) {
                    g_slave_registers[reg_addr] = reg_val;
                    uint8_t led_color = reg_addr - REG_BLUE_STATE;
                    led_set_state(led_color, (led_state_t)reg_val);
                    uart_console_print(&g_console_slave, "[Slave I2C] 暫存器寫入: Reg 0x%02X = 0x%02X\r\n", reg_addr, reg_val);
                } else {
                    uart_console_print(&g_console_slave, "[Slave I2C] 警告: 嘗試寫入唯讀暫存器 Reg 0x%02X\r\n", reg_addr);
                }
            } else {
                uart_console_print(&g_console_slave, "[Slave I2C] 錯誤: 暫存器位址 0x%02X 超出範圍\r\n", reg_addr);
            }
        }
        // 2. 處理標準自訂協定封包
        else if (rx_len >= 4 && packet_copy[0] == I2C_PACKET_HEADER) {
            uint8_t payload_len = packet_copy[1];
            if (rx_len == (payload_len + 4)) {
                uint8_t received_crc = packet_copy[rx_len - 1];
                uint8_t calc_crc = crc8_calc(packet_copy, rx_len - 1);

                if (calc_crc == received_crc) {
                    // 印出接收到的原始封包數據
                    uart_console_print(&g_console_slave, "[Slave I2C] 成功接收封包: [5A %02X %02X %02X %02X -> CRC:%02X]\r\n",
                                       packet_copy[1], packet_copy[2], packet_copy[3], packet_copy[4], packet_copy[5]);

                    uint8_t cmd = packet_copy[2];
                    
                    // 更新暫存器統計資料
                    g_slave_registers[REG_RX_PACKET_COUNT]++;
                    g_slave_registers[REG_LAST_COMMAND] = cmd;
                    g_slave_registers[REG_LAST_ERROR] = REG_ERR_NONE;

                    if (cmd == I2C_CMD_LED_CONTROL) {
                        uint8_t color = packet_copy[3];
                        uint8_t mode = packet_copy[4];

                        const char *color_str[] = {"Blue", "Green", "Red"};
                        const char *mode_str[] = {"OFF", "ON", "BLINK"};

                        if (color <= 2 && mode <= 2) {
                            // 更新 LED 狀態與硬體輸出
                            led_set_state(color, (led_state_t)mode);
                            
                            // 同步更新虛擬暫存器中的 LED 狀態
                            g_slave_registers[REG_BLUE_STATE + color] = mode;
                            
                            uart_console_print(&g_console_slave, "[Slave I2C] 成功執行 I2C 指令: 控制 %s 燈為 %s 狀態\r\n",
                                               color_str[color], mode_str[mode]);
                        } else {
                            uart_console_print(&g_console_slave, "[Slave I2C] 錯誤: 指令參數無效 (Color=%d, Mode=%d)\r\n", color, mode);
                        }
                    } else {
                        uart_console_print(&g_console_slave, "[Slave I2C] 未知指令 ID: 0x%02X\r\n", cmd);
                    }
                } else {
                    g_slave_registers[REG_CRC_FAIL_COUNT]++;
                    g_slave_registers[REG_LAST_ERROR] = REG_ERR_BAD_CRC;
                    uart_console_print(&g_console_slave, "[Slave I2C] CRC8 校驗失敗！計算值: 0x%02X, 收到值: 0x%02X\r\n",
                                       calc_crc, received_crc);
                }
            } else {
                g_slave_registers[REG_LAST_ERROR] = REG_ERR_BAD_LEN;
                uart_console_print(&g_console_slave, "[Slave I2C] 封包長度與 Length 欄位不符: rx_len=%d, expected=%d\r\n", 
                                   rx_len, payload_len + 4);
            }
        }
        // 3. 處理 1 位元組的讀取指標設定，靜默處理
        else if (rx_len == 1) {
            // 僅設定讀取指標，不進行任何錯誤列印
        }
        // 4. 其他無效資料
        else {
            if (rx_len > 0) {
                uart_console_print(&g_console_slave, "[Slave I2C] 接收到無效碎資料，長度: %d\r\n", rx_len);
            }
        }
    }

    if (g_slave_tx_complete) {
        // 保護臨界區避免與中斷衝突
        __disable_irq();
        g_slave_tx_complete = false;
        uint8_t tx_copy[5];
        memcpy(tx_copy, g_slave_tx_buf_copy, 5);
        __enable_irq();

        // 區分是狀態封包還是暫存器讀取封包
        if (tx_copy[0] == I2C_PACKET_HEADER) {
            uart_console_print(&g_console_slave, "[Slave I2C] 成功發送狀態封包: [5A %02X %02X %02X -> CRC:%02X]\r\n",
                               tx_copy[1], tx_copy[2], tx_copy[3], tx_copy[4]);
        } else {
            uart_console_print(&g_console_slave, "[Slave I2C] 成功發送暫存器 Reg 0x%02X 值: 0x%02X\r\n",
                               g_reg_pointer, tx_copy[0]);
        }
    }
}

// I2C Slave 中斷回呼 (由 FSP 中斷調用)
void cb_g_i2c_slave0(i2c_slave_callback_args_t *p_args) {
    switch (p_args->event) {
        case I2C_SLAVE_EVENT_RX_REQUEST: {
            // Master 開始發起寫入：直接提供大接收緩衝區讓 FSP 一次性讀取
            R_IIC_SLAVE_Read(&g_i2c_slave0_ctrl, g_slave_rx_buf, sizeof(g_slave_rx_buf));
            break;
        }

        case I2C_SLAVE_EVENT_RX_COMPLETE: {
            // 接收完成：取得實際接收到的位元組數
            g_slave_rx_idx = (uint8_t)p_args->bytes;
            
            if (g_slave_rx_idx == 1) {
                // 暫存器讀取的第一階段：設定讀取暫存器指標與 Pending 狀態
                g_reg_pointer = g_slave_rx_buf[0];
                g_reg_read_pending = true;
                extern volatile uint32_t g_system_ticks;
                g_reg_read_timestamp = g_system_ticks;
            }
            g_slave_packet_ready = true;
            break;
        }

        case I2C_SLAVE_EVENT_TX_REQUEST: {
            static uint8_t tx_buf[5]; // 宣告 static 緩衝區防 stack 釋放，且必須保持 5 欄位長度避免 MCU Stretch 鎖死
            extern volatile uint32_t g_system_ticks;

            // 判斷是否為暫存器讀取要求 (10ms 內有效)
            if (g_reg_read_pending && (g_system_ticks - g_reg_read_timestamp < 10)) {
                g_reg_read_pending = false;
                uint8_t val = 0;
                if (g_reg_pointer < 0x20) {
                    val = g_slave_registers[g_reg_pointer];
                }
                tx_buf[0] = val;
                tx_buf[1] = 0;
                tx_buf[2] = 0;
                tx_buf[3] = 0;
                tx_buf[4] = 0;
            } else {
                // 回傳預設 5 位元組 LED 狀態封包
                uint8_t blue = 0, green = 0, red = 0;
                led_get_all_states(&blue, &green, &red);
                tx_buf[0] = I2C_PACKET_HEADER;
                tx_buf[1] = blue;
                tx_buf[2] = green;
                tx_buf[3] = red;
                tx_buf[4] = crc8_calc(tx_buf, 4);
            }
            
            // 複製發送內容供主迴圈安全列印
            memcpy(g_slave_tx_buf_copy, tx_buf, 5);
            
            // 呼叫 FSP API 開始寫入
            R_IIC_SLAVE_Write(&g_i2c_slave0_ctrl, tx_buf, sizeof(tx_buf));
            break;
        }

        case I2C_SLAVE_EVENT_TX_COMPLETE:
            g_slave_tx_complete = true;
            break;

        default:
            break;
    }
}
