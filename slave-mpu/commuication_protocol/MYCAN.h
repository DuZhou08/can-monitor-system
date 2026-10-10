#ifndef __CAN_H
#define __CAN_H

void MYCAN_Init(void);
void MYCAN_Transmit(CanTxMsg *TxMsg);
uint8_t MYCAN_ReceiveFlag(void);
void MYCAN_Receive(CanRxMsg *RxMsg);


#endif
