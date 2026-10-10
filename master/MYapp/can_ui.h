#ifndef __CAN_UI_H
#define __CAN_UI_H

void CAN_UI_Init(void);      /* 创建页签+四页界面, LVGL 初始化后调用一次 */
void CAN_UI_Refresh(void);   /* 每帧刷新页面数据, 从 can_monitor 读 */

#endif
