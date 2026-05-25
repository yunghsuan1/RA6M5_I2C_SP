/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_usb_basic.h"
#include "r_usb_basic_api.h"
#include "r_usb_phid_api.h"
#include "r_iic_slave.h"
#include "r_i2c_slave_api.h"
#include "r_iic_master.h"
#include "r_i2c_master_api.h"
#include "r_sci_uart.h"
#include "r_uart_api.h"
FSP_HEADER
/* Basic on USB Instance. */
extern const usb_instance_t g_basic0;

/** Access the USB instance using these structures when calling API functions directly (::p_api is not used). */
extern usb_instance_ctrl_t g_basic0_ctrl;
extern const usb_cfg_t g_basic0_cfg;

#ifndef NULL
void NULL(void*);
#endif

#if 0 == BSP_CFG_RTOS
#ifndef usb_phid_callback
void usb_phid_callback(usb_callback_args_t*);
#endif
#endif

#if 2 == BSP_CFG_RTOS
#ifndef usb_phid_callback
void usb_phid_callback(usb_event_info_t *, usb_hdl_t, usb_onoff_t);
#endif
#endif
/** PHID Driver on USB Instance. */
/** I2C Slave on IIC Instance. */
extern const i2c_slave_instance_t g_i2c_slave2;

/** Access the I2C Slave instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_slave_instance_ctrl_t g_i2c_slave2_ctrl;
extern const i2c_slave_cfg_t g_i2c_slave2_cfg;

#ifndef cb_g_i2c_slave2
void cb_g_i2c_slave2(i2c_slave_callback_args_t *p_args);
#endif
/** I2C Slave on IIC Instance. */
extern const i2c_slave_instance_t g_i2c_slave0;

/** Access the I2C Slave instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_slave_instance_ctrl_t g_i2c_slave0_ctrl;
extern const i2c_slave_cfg_t g_i2c_slave0_cfg;

#ifndef cb_g_i2c_slave0
void cb_g_i2c_slave0(i2c_slave_callback_args_t *p_args);
#endif
/* I2C Master on IIC Instance. */
extern const i2c_master_instance_t g_i2c_master0;

/** Access the I2C Master instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_master_instance_ctrl_t g_i2c_master0_ctrl;
extern const i2c_master_cfg_t g_i2c_master0_cfg;

#ifndef cb_g_i2c_master0
void cb_g_i2c_master0(i2c_master_callback_args_t *p_args);
#endif
/** UART on SCI Instance. */
extern const uart_instance_t g_uart8;

/** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
extern sci_uart_instance_ctrl_t g_uart8_ctrl;
extern const uart_cfg_t g_uart8_cfg;
extern const sci_uart_extended_cfg_t g_uart8_cfg_extend;

#ifndef cb_g_uart8
void cb_g_uart8(uart_callback_args_t *p_args);
#endif
/** UART on SCI Instance. */
extern const uart_instance_t g_uart9;

/** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
extern sci_uart_instance_ctrl_t g_uart9_ctrl;
extern const uart_cfg_t g_uart9_cfg;
extern const sci_uart_extended_cfg_t g_uart9_cfg_extend;

#ifndef cb_g_uart9
void cb_g_uart9(uart_callback_args_t *p_args);
#endif
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */
