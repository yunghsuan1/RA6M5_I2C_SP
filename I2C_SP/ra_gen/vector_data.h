/* generated vector header file - do not edit */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
        extern "C" {
        #endif
/* Number of interrupts allocated */
#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (22)
#endif
/* ISR prototypes */
void sci_uart_rxi_isr(void);
void sci_uart_txi_isr(void);
void sci_uart_tei_isr(void);
void sci_uart_eri_isr(void);
void iic_master_rxi_isr(void);
void iic_master_txi_isr(void);
void iic_master_tei_isr(void);
void iic_master_eri_isr(void);
void iic_slave_rxi_isr(void);
void iic_slave_txi_isr(void);
void iic_slave_tei_isr(void);
void iic_slave_eri_isr(void);
void usbfs_interrupt_handler(void);
void usbfs_resume_handler(void);

/* Vector table allocations */
#define VECTOR_NUMBER_SCI9_RXI ((IRQn_Type) 0) /* SCI9 RXI (Receive data full) */
#define SCI9_RXI_IRQn          ((IRQn_Type) 0) /* SCI9 RXI (Receive data full) */
#define VECTOR_NUMBER_SCI9_TXI ((IRQn_Type) 1) /* SCI9 TXI (Transmit data empty) */
#define SCI9_TXI_IRQn          ((IRQn_Type) 1) /* SCI9 TXI (Transmit data empty) */
#define VECTOR_NUMBER_SCI9_TEI ((IRQn_Type) 2) /* SCI9 TEI (Transmit end) */
#define SCI9_TEI_IRQn          ((IRQn_Type) 2) /* SCI9 TEI (Transmit end) */
#define VECTOR_NUMBER_SCI9_ERI ((IRQn_Type) 3) /* SCI9 ERI (Receive error) */
#define SCI9_ERI_IRQn          ((IRQn_Type) 3) /* SCI9 ERI (Receive error) */
#define VECTOR_NUMBER_SCI8_RXI ((IRQn_Type) 4) /* SCI8 RXI (Receive data full) */
#define SCI8_RXI_IRQn          ((IRQn_Type) 4) /* SCI8 RXI (Receive data full) */
#define VECTOR_NUMBER_SCI8_TXI ((IRQn_Type) 5) /* SCI8 TXI (Transmit data empty) */
#define SCI8_TXI_IRQn          ((IRQn_Type) 5) /* SCI8 TXI (Transmit data empty) */
#define VECTOR_NUMBER_SCI8_TEI ((IRQn_Type) 6) /* SCI8 TEI (Transmit end) */
#define SCI8_TEI_IRQn          ((IRQn_Type) 6) /* SCI8 TEI (Transmit end) */
#define VECTOR_NUMBER_SCI8_ERI ((IRQn_Type) 7) /* SCI8 ERI (Receive error) */
#define SCI8_ERI_IRQn          ((IRQn_Type) 7) /* SCI8 ERI (Receive error) */
#define VECTOR_NUMBER_IIC0_RXI ((IRQn_Type) 8) /* IIC0 RXI (Receive data full) */
#define IIC0_RXI_IRQn          ((IRQn_Type) 8) /* IIC0 RXI (Receive data full) */
#define VECTOR_NUMBER_IIC0_TXI ((IRQn_Type) 9) /* IIC0 TXI (Transmit data empty) */
#define IIC0_TXI_IRQn          ((IRQn_Type) 9) /* IIC0 TXI (Transmit data empty) */
#define VECTOR_NUMBER_IIC0_TEI ((IRQn_Type) 10) /* IIC0 TEI (Transmit end) */
#define IIC0_TEI_IRQn          ((IRQn_Type) 10) /* IIC0 TEI (Transmit end) */
#define VECTOR_NUMBER_IIC0_ERI ((IRQn_Type) 11) /* IIC0 ERI (Transfer error) */
#define IIC0_ERI_IRQn          ((IRQn_Type) 11) /* IIC0 ERI (Transfer error) */
#define VECTOR_NUMBER_IIC1_RXI ((IRQn_Type) 12) /* IIC1 RXI (Receive data full) */
#define IIC1_RXI_IRQn          ((IRQn_Type) 12) /* IIC1 RXI (Receive data full) */
#define VECTOR_NUMBER_IIC1_TXI ((IRQn_Type) 13) /* IIC1 TXI (Transmit data empty) */
#define IIC1_TXI_IRQn          ((IRQn_Type) 13) /* IIC1 TXI (Transmit data empty) */
#define VECTOR_NUMBER_IIC1_TEI ((IRQn_Type) 14) /* IIC1 TEI (Transmit end) */
#define IIC1_TEI_IRQn          ((IRQn_Type) 14) /* IIC1 TEI (Transmit end) */
#define VECTOR_NUMBER_IIC1_ERI ((IRQn_Type) 15) /* IIC1 ERI (Transfer error) */
#define IIC1_ERI_IRQn          ((IRQn_Type) 15) /* IIC1 ERI (Transfer error) */
#define VECTOR_NUMBER_IIC2_RXI ((IRQn_Type) 16) /* IIC2 RXI (Receive data full) */
#define IIC2_RXI_IRQn          ((IRQn_Type) 16) /* IIC2 RXI (Receive data full) */
#define VECTOR_NUMBER_IIC2_TXI ((IRQn_Type) 17) /* IIC2 TXI (Transmit data empty) */
#define IIC2_TXI_IRQn          ((IRQn_Type) 17) /* IIC2 TXI (Transmit data empty) */
#define VECTOR_NUMBER_IIC2_TEI ((IRQn_Type) 18) /* IIC2 TEI (Transmit end) */
#define IIC2_TEI_IRQn          ((IRQn_Type) 18) /* IIC2 TEI (Transmit end) */
#define VECTOR_NUMBER_IIC2_ERI ((IRQn_Type) 19) /* IIC2 ERI (Transfer error) */
#define IIC2_ERI_IRQn          ((IRQn_Type) 19) /* IIC2 ERI (Transfer error) */
#define VECTOR_NUMBER_USBFS_INT ((IRQn_Type) 20) /* USBFS INT (USBFS interrupt) */
#define USBFS_INT_IRQn          ((IRQn_Type) 20) /* USBFS INT (USBFS interrupt) */
#define VECTOR_NUMBER_USBFS_RESUME ((IRQn_Type) 21) /* USBFS RESUME (USBFS resume interrupt) */
#define USBFS_RESUME_IRQn          ((IRQn_Type) 21) /* USBFS RESUME (USBFS resume interrupt) */
/* The number of entries required for the ICU vector table. */
#define BSP_ICU_VECTOR_NUM_ENTRIES (22)

#ifdef __cplusplus
        }
        #endif
#endif /* VECTOR_DATA_H */
