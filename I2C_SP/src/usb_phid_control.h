#ifndef USB_PHID_CONTROL_H
#define USB_PHID_CONTROL_H

#include "hal_data.h"
#include "r_usb_basic_api.h"
#include "r_usb_phid_api.h"
#include <stdbool.h>


// 初始化 USB 控制器
void usb_phid_init(void);

// USB 後台協議處理與字串發送狀態機
void usb_phid_process(void);

// 監測 P005 (S1) 與 P004 (S2) 按鍵輸入
void key_scan_process(void);

// FSP USB PHID 中斷事件 Callback
void usb_phid_callback(usb_callback_args_t * p_usb_event);


#endif // USB_PHID_CONTROL_H
