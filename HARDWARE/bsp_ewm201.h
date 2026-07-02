#ifndef __BSP_EWM201_H
#define __BSP_EWM201_H

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"


// Function Prototypes
void EWM201_Init(void);
void EWM201_PowerDown(uint8_t state); // 1 = Sleep, 0 = Active

#endif // __BSP_EWM201_H
