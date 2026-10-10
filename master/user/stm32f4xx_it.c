/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Main Interrupt Service Routines.
  *          Cortex-M4 exceptions + FreeRTOS SysTick hook.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/* SVC_Handler and PendSV_Handler are provided by the FreeRTOS port
   (vPortSVCHandler / xPortPendSVHandler, see FreeRTOSConfig.h) */

void DebugMon_Handler(void)
{
}

void SysTick_Handler(void)
{
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    xPortSysTickHandler();
  }
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f40_41xxx.s).                                          */
/******************************************************************************/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
