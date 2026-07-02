#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H

#include "stm32h7xx_it.h"
#include <stdint.h>

// Passive buzzer hardware configuration
// PI2 drives the transistor base through 1K, buzzer itself is switched by the transistor.
#define BUZZER_GPIO_PORT    GPIOI
#define BUZZER_GPIO_PIN     GPIO_PIN_2 // PI2 (BEEP)

typedef enum
{
    BUZZER_REMINDER_NONE = 0,
    BUZZER_REMINDER_LOW_BATTERY,
    BUZZER_REMINDER_GPS_RX
} Buzzer_Reminder_t;


// Function Prototypes
void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Toggle(void);
void Buzzer_Beep(uint32_t frequency, uint32_t duration_ms);

// Task Control API
void Buzzer_Suspend(void);
void Buzzer_Resume(void);
void Buzzer_SetAlarmActive(uint8_t active);
void Buzzer_RequestReminder(Buzzer_Reminder_t type);
void buzzer_task(void *argument);

#endif // __BSP_BUZZER_H
