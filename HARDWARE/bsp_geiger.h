#ifndef __BSP_GEIGER_H
#define __BSP_GEIGER_H

#include "stm32h7xx_hal.h"

// Geiger Counter Configuration
// Connect the pulse output pin to an external interrupt pin
#define GEIGER_GPIO_PORT    GPIOA
#define GEIGER_GPIO_PIN     GPIO_PIN_8
#define GEIGER_IRQn         EXTI9_5_IRQn

// Conversion Factor (J305 specific, needs calibration)
// Typically around 153 CPM = 1 uSv/h, so 1 CPM = 1/153 uSv/h = 0.0065 uSv/h
#define GEIGER_CPM_TO_USV   0.0065f 

// Function Prototypes
void Geiger_Init(void);
void Geiger_Callback(void); // Call this from HAL_GPIO_EXTI_Callback
uint32_t Geiger_GetCPM(void);
float Geiger_GetuSv(void);
void Geiger_Update(void); // Call periodically (e.g. every second) in main loop or timer interrupt
uint32_t Geiger_GetPulseCount(void);

// Task Control API
void Geiger_Suspend(void);
void Geiger_Resume(void);
void geiger_task(void *argument);

#endif // __BSP_GEIGER_H
