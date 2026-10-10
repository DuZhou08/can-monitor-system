#ifndef __MYAPP_H
#define __MYAPP_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

/*hardware function*/
#include "LED.h"
#include "OLED.h"
#include "MYADC.h"
#include "MYCAN.h"
/*                 */

#include "semphr.h"

void FreeRTOS_Start(void);

#endif
