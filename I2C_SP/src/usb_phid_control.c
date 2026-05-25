#include "usb_phid_control.h"
#include "uart_console.h"
#include <string.h>

/* 私有函式前置宣告 */
static void deinit_usb(void);

/* 巨集定義 */

#define USB_RECEIVE_REPORT_DESCRIPTOR    (76U)              // 報告描述子長度
#define USB_RECEIVE_HID_DESCRIPTOR       (9U)               // HID 描述子長度
#define DATA_LEN                         (8U)               // 鍵盤 report 長度
#define SIZE_NUM                         (2U)               // NumLock 資料長度
#define CD_LENGTH                        (18U)              // 配置描述子前段長度
#define IDLE_VAL_INDEX                   (1U)               
#define BUFF_SIZE                        (16U)              
#define ALIGN                            (4U)               

/* 外部變數引用 */
extern uint8_t g_apl_configuration[];
extern uint8_t g_apl_report[];
extern volatile uint32_t g_system_ticks;

/* 佇列結構已移除，改由 R_USB_EventGet 輪詢驅動 */

/* 狀態機變數 */
typedef enum {
    SEND_STATE_IDLE,
    SEND_STATE_KEY_DOWN,
    SEND_STATE_WAIT_KEY_DOWN,
    SEND_STATE_KEY_UP,
    SEND_STATE_WAIT_KEY_UP
} send_state_t;

static send_state_t g_send_state = SEND_STATE_IDLE;
static char g_send_buffer[32] = {0};
static uint8_t g_send_len = 0;
static uint8_t g_send_idx = 0;

static volatile bool g_usb_write_busy = false;
static volatile bool g_usb_configured = false;

/* USB 暫存變數 */
static uint8_t send_data[BUFF_SIZE] BSP_ALIGN_VARIABLE(ALIGN);
static uint8_t g_buf[DATA_LEN]  = {0}; /* 全零的放開按鍵 Report */
static uint8_t g_data[DATA_LEN] = {0}; /* 按鍵發送 Report */
static uint16_t g_numlock = 0;
static uint8_t g_idle = 0;

/* 將 ASCII 字元轉換為 USB HID 鍵盤的使用者碼 (Usage ID) 及修飾鍵 (Modifier) */
static bool char_to_usb_keycode(char c, uint8_t *p_keycode, uint8_t *p_modifier) {
    *p_modifier = 0; // 預設無修飾鍵 (如 Shift)
    
    if (c >= 'a' && c <= 'z') {
        *p_keycode = (uint8_t)(0x04 + (c - 'a'));
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        *p_keycode = (uint8_t)(0x04 + (c - 'A'));
        *p_modifier = 0x02; // Left Shift
        return true;
    }
    if (c >= '1' && c <= '9') {
        *p_keycode = (uint8_t)(0x1E + (c - '1'));
        return true;
    }
    if (c == '0') {
        *p_keycode = 0x27;
        return true;
    }
    if (c == '_') {
        *p_keycode = 0x2D;   // '-' 鍵，搭配 Shift 即為底線 '_'
        *p_modifier = 0x02;  // Left Shift
        return true;
    }
    if (c == ' ') {
        *p_keycode = 0x2C;   // 空白鍵
        return true;
    }
    return false;
}

/* 初始化 USB 控制器 */
void usb_phid_init(void) {
    g_send_state = SEND_STATE_IDLE;
    g_usb_write_busy = false;
    g_usb_configured = false;
    
    fsp_err_t err = R_USB_Open(&g_basic0_ctrl, &g_basic0_cfg);
    if (FSP_SUCCESS != err) {
        uart_console_print(&g_console_master, "[USB ERROR] R_USB_Open 失敗: 0x%X\r\n", err);
    } else {
        uart_console_print(&g_console_master, "[USB] R_USB_Open 成功\r\n");
    }
}

/* 觸發發送字串 */
static void usb_phid_send_string(const char *str) {
    if (!g_usb_configured) {
        uart_console_print(&g_console_master, "[USB] 傳送失敗: USB 尚未完成列舉設定\r\n");
        return;
    }
    if (g_send_state != SEND_STATE_IDLE) {
        return; // 忙碌中，忽略新按鍵
    }
    
    strncpy(g_send_buffer, str, sizeof(g_send_buffer) - 1);
    g_send_buffer[sizeof(g_send_buffer) - 1] = '\0';
    g_send_len = (uint8_t)strlen(g_send_buffer);
    g_send_idx = 0;
    g_send_state = SEND_STATE_KEY_DOWN;
    
    uart_console_print(&g_console_master, "[USB] 開始模擬鍵盤輸入: %s\r\n", g_send_buffer);
}

/* 處理 USB 事件與字串發送狀態機 */
void usb_phid_process(void) {
    fsp_err_t err = FSP_SUCCESS;
    usb_status_t usb_event = USB_STATUS_NONE;
    usb_event_info_t ev = {0};

    // 1. 呼叫 R_USB_EventGet 取得底層 USB 事件佇列內容 (同時也驅動了內部的 PCD Task)
    err = R_USB_EventGet(&ev, &usb_event);

    // 2. 處理 USB 協定事件
    if (FSP_SUCCESS == err) {
        switch (usb_event) {
            case USB_STATUS_CONFIGURED: {
                g_usb_configured = true;
                uart_console_print(&g_console_master, "[USB] 狀態: CONFIGURED (設備列舉完成)\r\n");
                uart_console_print(&g_console_slave, "[USB] 狀態: CONFIGURED\r\n");
                break;
            }
            case USB_STATUS_REQUEST: {
                /* 處理 Host 端的 Setup 請求 */
                uint16_t req_type = ev.setup.request_type & USB_BREQUEST;
                if (USB_SET_REPORT == req_type) {
                    err = R_USB_PeriControlDataGet(&g_basic0_ctrl, (uint8_t *)&g_numlock, SIZE_NUM);
                    if (FSP_SUCCESS != err) {
                        deinit_usb();
                    }
                }
                else if (USB_GET_DESCRIPTOR == req_type) {
                    if (USB_GET_REPORT_DESCRIPTOR == ev.setup.request_value) {
                        err = R_USB_PeriControlDataSet(&g_basic0_ctrl, (uint8_t *)g_apl_report, USB_RECEIVE_REPORT_DESCRIPTOR);
                        if (FSP_SUCCESS != err) {
                            deinit_usb();
                        }
                    }
                    else if (USB_GET_HID_DESCRIPTOR == ev.setup.request_value) {
                        for (uint8_t i = 0; i < USB_RECEIVE_HID_DESCRIPTOR; i++) {
                            send_data[i] = g_apl_configuration[CD_LENGTH + i];
                        }
                        err = R_USB_PeriControlDataSet(&g_basic0_ctrl, send_data, USB_RECEIVE_HID_DESCRIPTOR);
                        if (FSP_SUCCESS != err) {
                            deinit_usb();
                        }
                    }
                }
                else if (USB_SET_IDLE == req_type) {
                    uint8_t *p_idle_value = (uint8_t *)&ev.setup.request_value;
                    g_idle = p_idle_value[IDLE_VAL_INDEX];
                    err = R_USB_PeriControlStatusSet(&g_basic0_ctrl, USB_SETUP_STATUS_ACK);
                    if (FSP_SUCCESS != err) {
                        deinit_usb();
                    }
                }
                break;
            }
            case USB_STATUS_REQUEST_COMPLETE: {
                /* 請求處理完成，發送初始 NULL 封包 */
                uint16_t req_type = ev.setup.request_type & USB_BREQUEST;
                if (USB_SET_IDLE == req_type) {
                    uint8_t *p_idle_value = (uint8_t *)&ev.setup.request_value;
                    g_idle = p_idle_value[IDLE_VAL_INDEX];
                }
                else if (USB_SET_PROTOCOL == req_type) {
                    // 暫不處理
                }
                else if (USB_SET_REPORT == req_type) {
                    // SetReport 完成，如 PC 控制 KeyBoard 的 NumLock 指示燈
                }
                else {
                    // 發送第 1 包全零資料以喚醒傳輸
                    g_usb_write_busy = true;
                    err = R_USB_Write(&g_basic0_ctrl, g_buf, DATA_LEN, USB_CLASS_PHID);
                    if (FSP_SUCCESS != err) {
                        g_usb_write_busy = false;
                        deinit_usb();
                    }
                }
                break;
            }
            case USB_STATUS_WRITE_COMPLETE: {
                g_usb_write_busy = false;
                break;
            }
            case USB_STATUS_SUSPEND: {
                uart_console_print(&g_console_master, "[USB] 狀態: SUSPEND (主機端暫停)\r\n");
                break;
            }
            case USB_STATUS_DETACH: {
                g_usb_configured = false;
                g_usb_write_busy = false;
                g_send_state = SEND_STATE_IDLE;
                uart_console_print(&g_console_master, "[USB] 狀態: DETACH (連接中斷)\r\n");
                break;
            }
            default:
                break;
        }
    }

    // 3. 處理字串發送狀態機
    if (g_usb_configured) {
        switch (g_send_state) {
            case SEND_STATE_IDLE:
                break;
                
            case SEND_STATE_KEY_DOWN: {
                if (!g_usb_write_busy) {
                    uint8_t keycode = 0;
                    uint8_t modifier = 0;
                    char c = g_send_buffer[g_send_idx];
                    
                    if (char_to_usb_keycode(c, &keycode, &modifier)) {
                        memset(g_data, 0, DATA_LEN);
                        g_data[0] = modifier;
                        g_data[2] = keycode;
                        
                        g_usb_write_busy = true;
                        err = R_USB_Write(&g_basic0_ctrl, g_data, DATA_LEN, USB_CLASS_PHID);
                        if (FSP_SUCCESS == err) {
                            g_send_state = SEND_STATE_WAIT_KEY_DOWN;
                        } else {
                            g_usb_write_busy = false;
                            uart_console_print(&g_console_master, "[USB ERROR] 按鍵發送失敗: 0x%X\r\n", err);
                            g_send_state = SEND_STATE_IDLE;
                        }
                    } else {
                        // 無法解析的字元，直接跳過，進到下一個
                        g_send_idx++;
                        if (g_send_idx >= g_send_len) {
                            g_send_state = SEND_STATE_IDLE;
                        }
                    }
                }
                break;
            }
            
            case SEND_STATE_WAIT_KEY_DOWN: {
                if (!g_usb_write_busy) {
                    g_send_state = SEND_STATE_KEY_UP;
                }
                break;
            }
            
            case SEND_STATE_KEY_UP: {
                if (!g_usb_write_busy) {
                    g_usb_write_busy = true;
                    // 發送全零 NULL Report 代表釋放按鍵
                    err = R_USB_Write(&g_basic0_ctrl, g_buf, DATA_LEN, USB_CLASS_PHID);
                    if (FSP_SUCCESS == err) {
                        g_send_state = SEND_STATE_WAIT_KEY_UP;
                    } else {
                        g_usb_write_busy = false;
                        g_send_state = SEND_STATE_IDLE;
                    }
                }
                break;
            }
            
            case SEND_STATE_WAIT_KEY_UP: {
                if (!g_usb_write_busy) {
                    g_send_idx++;
                    if (g_send_idx < g_send_len) {
                        g_send_state = SEND_STATE_KEY_DOWN;
                    } else {
                        g_send_state = SEND_STATE_IDLE;
                        uart_console_print(&g_console_master, "[USB] 模擬鍵盤輸入完畢\r\n");
                    }
                }
                break;
            }
        }
    }
}

/* 關閉 USB 控制器 */
static void deinit_usb(void) {
    R_USB_Close(&g_basic0_ctrl);
    g_usb_configured = false;
    g_usb_write_busy = false;
    g_send_state = SEND_STATE_IDLE;
}

/* 偵測 S1 鍵 (P005) 與 S2 鍵 (P004) 按下邊緣並消抖 */
void key_scan_process(void) {
    static uint32_t last_scan_tick = 0;
    static bsp_io_level_t last_s1_state = BSP_IO_LEVEL_HIGH;
    static bsp_io_level_t last_s2_state = BSP_IO_LEVEL_HIGH;
    
    // 每 20ms 進行一次按鍵掃描 (軟體去抖動)
    if (g_system_ticks - last_scan_tick < 20) {
        return;
    }
    last_scan_tick = g_system_ticks;
    
    bsp_io_level_t s1_level = BSP_IO_LEVEL_HIGH;
    bsp_io_level_t s2_level = BSP_IO_LEVEL_HIGH;
    
    // 讀取開發板 S1 (P005) 與 S2 (P004) GPIO 引腳
    g_ioport.p_api->pinRead(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_05, &s1_level);
    g_ioport.p_api->pinRead(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_04, &s2_level);
    
    // 偵測 S1 下降緣 (按下)
    if (last_s1_state == BSP_IO_LEVEL_HIGH && s1_level == BSP_IO_LEVEL_LOW) {
        usb_phid_send_string("input_s1");
    }
    last_s1_state = s1_level;
    
    // 偵測 S2 下降緣 (按下)
    if (last_s2_state == BSP_IO_LEVEL_HIGH && s2_level == BSP_IO_LEVEL_LOW) {
        usb_phid_send_string("input_s2");
    }
    last_s2_state = s2_level;
}

/* USB 中斷事件處理回呼 (FSP 中斷觸發時調用) */
void usb_phid_callback(usb_callback_args_t * p_usb_event) {
    FSP_PARAMETER_NOT_USED(p_usb_event);
}
