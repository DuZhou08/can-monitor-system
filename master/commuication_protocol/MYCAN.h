#ifndef __MYCAN_H
#define __MYCAN_H

#include "stm32f4xx.h"

#define CAN_GPIO        GPIOA
#define CAN_TX_PIN      GPIO_Pin_12
#define CAN_RX_PIN      GPIO_Pin_11

/* ================================================================
 *  临时自测: 内部回环 (1=开, 0=正常总线模式)
 *  用途: 无需任何外部接线, 自发自收验证 F407 CAN 收发全链路.
 *  判定: Diag 页 RX FRAMES 增长 + Node 页 0x300 在线 = 软件链路 OK.
 *  自测通过后把本宏置 0, 恢复 CAN_Mode_Normal 真实收总线.
 * ================================================================ */
#define MYCAN_TEST_LOOPBACK 0

/* 控制器错误状态 (与 cm_bus_t.bus_state 语义一致) */
#define MYCAN_ST_NORMAL   0
#define MYCAN_ST_PASSIVE  1
#define MYCAN_ST_BUSOFF   2

typedef struct {
	uint8_t state;    /* MYCAN_ST_*: 由 ESR 的 BOF/EPVF 标志得出 */
	uint8_t tec;      /* 发送错误计数 0~255 */
	uint8_t rec;      /* 接收错误计数 0~255 */
	uint8_t lec;      /* 最后错误码: 0无 1填充 2格式 3ACK 4隐性位 5显性位 6CRC 7SW */
} mycan_diag_t;

void MYCAN_Init(void);
void MYCAN_Transmit(CanTxMsg *TxMsg);
uint8_t MYCAN_ReceiveFlag(void);
void MYCAN_Receive(CanRxMsg *RxMsg);
void MYCAN_GetDiag(mycan_diag_t *d);

#endif
