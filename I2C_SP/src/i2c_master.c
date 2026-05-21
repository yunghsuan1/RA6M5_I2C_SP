#include "i2c_master.h"
#include "crc8.h"
#include "uart_console.h"

// 全域 Master 傳送與接收同步標誌
volatile bool g_i2c_master_tx_complete = false;
volatile bool g_i2c_master_rx_complete = false;
volatile bool g_i2c_master_err = false;

// 初始化 I2C Master 模組
void i2c_master_init(void) {
    g_i2c_master_tx_complete = false;
    g_i2c_master_rx_complete = false;
    g_i2c_master_err = false;
    // 開啟 FSP I2C Master 驅動 (預期在 FSP 中命名為 g_i2c_master0)
    R_IIC_MASTER_Open(&g_i2c_master0_ctrl, &g_i2c_master0_cfg);
}

// 發送 LED 控制封包
fsp_err_t i2c_master_send_led_cmd(uint8_t led_color, uint8_t led_mode) {
    const char *color_str[] = {"Blue", "Green", "Red"};
    const char *mode_str[] = {"OFF", "ON", "BLINK"};
    
    if (led_color <= 2 && led_mode <= 2) {
        uart_console_print(&g_console_master, "[Master UART -> I2C] 轉發控制指令: 將 %s 燈設為 %s\r\n", 
                           color_str[led_color], mode_str[led_mode]);
    }

    // 封包格式：Header(0x5A) | Length(2) | Command(0x01) | Color(1 byte) | Mode(1 byte) | CRC8(1 byte)
    static uint8_t tx_buf[6];
    tx_buf[0] = I2C_PACKET_HEADER;
    tx_buf[1] = 2; // Payload 長度
    tx_buf[2] = I2C_CMD_LED_CONTROL;
    tx_buf[3] = led_color;
    tx_buf[4] = led_mode;
    tx_buf[5] = crc8_calc(tx_buf, 5); // 計算前面 5 位元組的 CRC8

    g_i2c_master_tx_complete = false;
    g_i2c_master_err = false;

    // 非阻塞發送 I2C 資料包 (Slave Address 由 FSP 設定或在 Write 中指定，這裡帶入預設)
    fsp_err_t err = R_IIC_MASTER_Write(&g_i2c_master0_ctrl, tx_buf, sizeof(tx_buf), false);
    if (FSP_SUCCESS != err) {
        uart_console_print(&g_console_master, "[Master I2C] 發送啟動失敗, 錯誤碼: 0x%X\r\n", err);
        return err;
    }

    // 主迴圈中進行帶逾時的忙碌等待 (Bare-metal 同步化)
    uint32_t timeout = 50000; // 約 50ms 逾時
    while (!g_i2c_master_tx_complete && !g_i2c_master_err && timeout > 0) {
        timeout--;
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    }

    if (g_i2c_master_err) {
        uart_console_print(&g_console_master, "[Master I2C] 發送被硬體中止 (NAK 或 匯流排錯誤)\r\n");
        return FSP_ERR_ABORTED;
    }

    if (timeout == 0) {
        uart_console_print(&g_console_master, "[Master I2C] 發送逾時\r\n");
        return FSP_ERR_TIMEOUT;
    }

    // 發送成功印出封包內容供除錯
    uart_console_print(&g_console_master, "[Master I2C] 成功發送封包: [5A %02X %02X %02X %02X -> CRC:%02X]\r\n",
                       tx_buf[1], tx_buf[2], tx_buf[3], tx_buf[4], tx_buf[5]);

    return FSP_SUCCESS;
}

// I2C Master 中斷回呼
void cb_g_i2c_master0(i2c_master_callback_args_t *p_args) {
    switch (p_args->event) {
        case I2C_MASTER_EVENT_TX_COMPLETE:
            g_i2c_master_tx_complete = true;
            break;
        case I2C_MASTER_EVENT_RX_COMPLETE:
            g_i2c_master_rx_complete = true;
            break;
        case I2C_MASTER_EVENT_ABORTED:
            g_i2c_master_err = true;
            break;
        default:
            break;
    }
}

// 向 Slave 讀取 LED 狀態
fsp_err_t i2c_master_read_status(uint8_t *p_blue, uint8_t *p_green, uint8_t *p_red) {
    static uint8_t rx_buf[5]; // Header(0x5A) | Blue | Green | Red | CRC8
    memset(rx_buf, 0, sizeof(rx_buf));
    
    g_i2c_master_rx_complete = false;
    g_i2c_master_err = false;
    
    // 發起 I2C 讀取傳輸 (要求 5 位元組)
    fsp_err_t err = R_IIC_MASTER_Read(&g_i2c_master0_ctrl, rx_buf, sizeof(rx_buf), false);
    if (FSP_SUCCESS != err) {
        uart_console_print(&g_console_master, "[Master I2C] 讀取啟動失敗, 錯誤碼: 0x%X\r\n", err);
        return err;
    }
    
    // 等待接收完成或出錯
    uint32_t timeout = 50000; // 約 50ms 逾時
    while (!g_i2c_master_rx_complete && !g_i2c_master_err && timeout > 0) {
        timeout--;
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    }
    
    if (g_i2c_master_err) {
        uart_console_print(&g_console_master, "[Master I2C] 讀取被硬體中止 (NAK 或 匯流排錯誤)\r\n");
        return FSP_ERR_ABORTED;
    }
    
    if (timeout == 0) {
        uart_console_print(&g_console_master, "[Master I2C] 讀取逾時\r\n");
        return FSP_ERR_TIMEOUT;
    }
    
    // 校驗封包
    if (rx_buf[0] != I2C_PACKET_HEADER) {
        uart_console_print(&g_console_master, "[Master I2C] 讀取封包 Header 錯誤: 0x%02X\r\n", rx_buf[0]);
        return FSP_ERR_INVALID_DATA;
    }
    
    uint8_t received_crc = rx_buf[4];
    uint8_t calc_crc = crc8_calc(rx_buf, 4);
    if (calc_crc != received_crc) {
        uart_console_print(&g_console_master, "[Master I2C] 讀取封包 CRC8 校驗失敗！計算值: 0x%02X, 收到值: 0x%02X\r\n",
                           calc_crc, received_crc);
        return FSP_ERR_INVALID_DATA;
    }
    
    // 校驗成功，賦值
    *p_blue = rx_buf[1];
    *p_green = rx_buf[2];
    *p_red = rx_buf[3];
    
    // 印出原始接收封包除錯訊息
    uart_console_print(&g_console_master, "[Master I2C] 成功讀取狀態封包: [5A %02X %02X %02X -> CRC:%02X]\r\n",
                       rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4]);
    
    return FSP_SUCCESS;
}
