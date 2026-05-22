#include "i2c_slave2.h"
#include "crc8.h"
#include "uart_console.h"
#include <string.h>

// 全域傳送與接收變數 (與中斷同步)
static volatile bool g_slave2_tx_complete = false;
static uint8_t g_slave2_tx_buf_copy[5];

// 初始化 I2C Slave 2 模組，並透過暫存器喚醒 TSN 與 ADC0
void i2c_slave2_init(void) {
    g_slave2_tx_complete = false;
    memset(g_slave2_tx_buf_copy, 0, sizeof(g_slave2_tx_buf_copy));

    // 1. 喚醒 TSN 與 ADC120 模組 (解除 Module Stop 狀態)
    R_BSP_MODULE_START(FSP_IP_TSN, 0); // 取消 TSN Module Stop (MSTPD22 = 0)
    R_BSP_MODULE_START(FSP_IP_ADC, 0); // 取消 ADC120 Module Stop (MSTPD16 = 0)

    // 2. 啟用溫度感測器 (TSN)
    R_TSN_CTRL->TSCR_b.TSEN = 1;       // 啟用 TSN
    R_BSP_SoftwareDelay(30, BSP_DELAY_UNITS_MICROSECONDS); // 延遲 >= 30us 確保穩定
    R_TSN_CTRL->TSCR_b.TSOE = 1;       // 啟用溫度感測器輸出至 ADC

    // 3. 設定 ADC120 暫存器
    R_ADC0->ADCSR_b.ADST = 0;          // 確保 ADC 停止
    R_ADC0->ADCSR_b.ADCS = 0;          // 單次掃描模式 (Single Scan Mode)
    R_ADC0->ADCSR_b.ADHSC = 1;         // 高速轉換模式
    
    // 清除普通類比通道選擇，避免掃描到其他引腳
    R_ADC0->ADANSA[0] = 0;
    R_ADC0->ADANSA[1] = 0;

    // 選擇溫度感測器為轉換來源 (TSSA = 1, TSSAD = 0 表示不加算/平均)
    R_ADC0->ADEXICR = 0x0100U;

    // 設定取樣時間狀態數為最高 0xFF (在 50 MHz 下約 5.1 us)，滿足 TSN >= 4.15 us 的需求
    R_ADC0->ADSSTRT = 0xFF;

    // 4. 開啟 FSP I2C Slave2 驅動 (IIC Channel 2)
    R_IIC_SLAVE_Open(&g_i2c_slave2_ctrl, &g_i2c_slave2_cfg);
}

// 讀取真實 TSN 暫存器並進行溫度運算
float read_tsn_temp(void) {
    // 1. 觸發 A/D 轉換
    R_ADC0->ADCSR_b.ADST = 1;

    // 2. 輪詢等待轉換完成
    uint32_t timeout = 100000;
    while (R_ADC0->ADCSR_b.ADST && --timeout);

    if (timeout == 0) {
        return -99.0f; // 轉換超時，返回錯誤值
    }

    // 3. 讀取 A/D 溫度感測器數據暫存器 (ADTSDR)
    uint16_t adc_raw = R_ADC0->ADTSDR & 0x0FFF;

    // 4. 讀取出廠 127 度校準值 (TSCDR)
    uint32_t cal127_raw = R_TSN_CAL->TSCDR & 0xFFFFUL;

    // 5. 進行溫度計算
    float cal127 = (float)cal127_raw;
    float v_cal127 = (cal127 * 3.3f) / 4096.0f;
    float v_s = ((float)adc_raw * 3.3f) / 4096.0f;
    float slope = (float)BSP_FEATURE_TSN_SLOPE / 1000000.0f; // 4000UL -> 0.004 V/degC

    float temp = ((v_s - v_cal127) / slope) + 127.0f;
    return temp;
}

// 主輪詢程序：異步處理 I2C2 傳送成功之日誌列印
void i2c_slave2_process(void) {
    if (g_slave2_tx_complete) {
        __disable_irq();
        g_slave2_tx_complete = false;
        uint8_t tx_copy[5];
        memcpy(tx_copy, g_slave2_tx_buf_copy, 5);
        __enable_irq();

        // 印出發送出的封包細節
        uart_console_print(&g_console_slave, "[Slave 2 I2C] 成功發送 TSN 遙測封包: [%02X %02X %02X %02X -> CRC:%02X]\r\n",
                           tx_copy[0], tx_copy[1], tx_copy[2], tx_copy[3], tx_copy[4]);
    }
}

// I2C Slave 2 中斷回呼函式
void cb_g_i2c_slave2(i2c_slave_callback_args_t *p_args) {
    switch (p_args->event) {
        case I2C_SLAVE_EVENT_RX_REQUEST: {
            // 當 Master 進行 scan 探測寫入時，必須讀取資料以維持 FSP 狀態機同步
            static uint8_t dummy_rx_buf[16];
            R_IIC_SLAVE_Read(&g_i2c_slave2_ctrl, dummy_rx_buf, sizeof(dummy_rx_buf));
            break;
        }

        case I2C_SLAVE_EVENT_RX_COMPLETE: {
            break;
        }

        case I2C_SLAVE_EVENT_TX_REQUEST: {
            static uint8_t tx_buf[5]; // Header(0x5A) | Temp_Int | Temp_Dec | Reserved(0) | CRC8
            
            float temp = read_tsn_temp();
            if (temp < -100.0f || temp > 150.0f) {
                temp = 0.0f;
            }

            uint8_t temp_int = (uint8_t)temp;
            uint8_t temp_dec = (uint8_t)(((temp - (float)temp_int) * 100.0f) + 0.5f);
            if (temp_dec >= 100) {
                temp_int += 1;
                temp_dec = 0;
            }

            tx_buf[0] = I2C_PACKET_HEADER;
            tx_buf[1] = temp_int;
            tx_buf[2] = temp_dec;
            tx_buf[3] = 0; // Reserved
            tx_buf[4] = crc8_calc(tx_buf, 4);

            // 複製一份資料給主迴圈列印
            memcpy(g_slave2_tx_buf_copy, tx_buf, 5);

            // 送出封包
            R_IIC_SLAVE_Write(&g_i2c_slave2_ctrl, tx_buf, sizeof(tx_buf));
            break;
        }

        case I2C_SLAVE_EVENT_TX_COMPLETE: {
            g_slave2_tx_complete = true;
            break;
        }

        default:
            break;
    }
}
