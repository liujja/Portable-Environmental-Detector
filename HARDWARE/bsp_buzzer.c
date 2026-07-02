#include "bsp_buzzer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sys_task.h"
#include "stm32h7xx_hal.h"

#if ENABLE_BSP_BUZZER
#include "cmsis_os2.h"

#define BUZZER_SERVICE_SLICE_MS     5U
#define BUZZER_CARRIER_FREQ_HZ      2700U
#define BUZZER_HW_TEST_MODE         0U

typedef enum
{
    BUZZER_PATTERN_NONE = 0,
    BUZZER_PATTERN_ALARM,
    BUZZER_PATTERN_LOW_BATTERY,
    BUZZER_PATTERN_GPS_RX
} Buzzer_PatternId_t;

typedef struct
{
    const uint16_t *steps_ms;
    uint8_t step_count;
    uint8_t repeat;
    uint16_t tone_hz;
} Buzzer_Pattern_t;

static const uint16_t buzzer_alarm_steps[] = {120U, 120U, 120U, 700U};
static const uint16_t buzzer_low_battery_steps[] = {260U, 160U, 260U, 1200U};
static const uint16_t buzzer_gps_rx_steps[] = {70U, 70U, 70U, 180U};

static const Buzzer_Pattern_t buzzer_alarm_pattern =
{
    buzzer_alarm_steps,
    (uint8_t)(sizeof(buzzer_alarm_steps) / sizeof(buzzer_alarm_steps[0])),
    1U,
    BUZZER_CARRIER_FREQ_HZ
};

static const Buzzer_Pattern_t buzzer_low_battery_pattern =
{
    buzzer_low_battery_steps,
    (uint8_t)(sizeof(buzzer_low_battery_steps) / sizeof(buzzer_low_battery_steps[0])),
    0U,
    BUZZER_CARRIER_FREQ_HZ
};

static const Buzzer_Pattern_t buzzer_gps_rx_pattern =
{
    buzzer_gps_rx_steps,
    (uint8_t)(sizeof(buzzer_gps_rx_steps) / sizeof(buzzer_gps_rx_steps[0])),
    0U,
    BUZZER_CARRIER_FREQ_HZ
};

static volatile uint8_t buzzer_suspended = 0U;
static volatile uint8_t buzzer_alarm_active = 0U;
static volatile uint8_t buzzer_low_battery_pending = 0U;
static volatile uint8_t buzzer_gps_rx_pending = 0U;

static Buzzer_PatternId_t buzzer_current_pattern = BUZZER_PATTERN_NONE;
static uint8_t buzzer_current_step = 0U;
static uint32_t buzzer_step_deadline = 0U;
static uint32_t buzzer_cycles_per_us = 0U;

static const Buzzer_Pattern_t *Buzzer_GetPattern(Buzzer_PatternId_t id)
{
    switch (id)
    {
    case BUZZER_PATTERN_ALARM:
        return &buzzer_alarm_pattern;

    case BUZZER_PATTERN_LOW_BATTERY:
        return &buzzer_low_battery_pattern;

    case BUZZER_PATTERN_GPS_RX:
        return &buzzer_gps_rx_pattern;

    default:
        return NULL;
    }
}

static void Buzzer_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOI_CLK_ENABLE();

    GPIO_InitStruct.Pin = BUZZER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
}

static void Buzzer_DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    buzzer_cycles_per_us = SystemCoreClock / 1000000U;
    if (buzzer_cycles_per_us == 0U)
    {
        buzzer_cycles_per_us = 1U;
    }
}

static void Buzzer_DelayUs(uint32_t us)
{
    uint32_t start;
    uint32_t target_cycles;

    if (us == 0U)
    {
        return;
    }

    start = DWT->CYCCNT;
    target_cycles = us * buzzer_cycles_per_us;

    while ((uint32_t)(DWT->CYCCNT - start) < target_cycles)
    {
    }
}

static void Buzzer_OutputTone(uint16_t frequency, uint32_t duration_ms)
{
    uint32_t half_period_us;
    uint32_t cycles;
    uint32_t i;

    if ((frequency == 0U) || (duration_ms == 0U))
    {
        return;
    }

    /* 50% duty square wave for passive buzzer drive. */
    half_period_us = 500000U / (uint32_t)frequency;
    if (half_period_us == 0U)
    {
        half_period_us = 1U;
    }

    cycles = ((uint32_t)frequency * duration_ms) / 1000U;
    if (cycles == 0U)
    {
        cycles = 1U;
    }

    for (i = 0U; i < cycles; i++)
    {
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
        Buzzer_DelayUs(half_period_us);
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
        Buzzer_DelayUs(half_period_us);
    }
}

static void Buzzer_StopPattern(void)
{
    buzzer_current_pattern = BUZZER_PATTERN_NONE;
    buzzer_current_step = 0U;
    buzzer_step_deadline = 0U;
    Buzzer_Off();
}

static void Buzzer_StartPattern(Buzzer_PatternId_t id, uint32_t now_tick)
{
    const Buzzer_Pattern_t *pattern = Buzzer_GetPattern(id);

    if ((pattern == NULL) || (pattern->step_count == 0U))
    {
        Buzzer_StopPattern();
        return;
    }

    buzzer_current_pattern = id;
    buzzer_current_step = 0U;
    buzzer_step_deadline = now_tick + pattern->steps_ms[0];
}

static void Buzzer_AdvancePattern(uint32_t now_tick)
{
    const Buzzer_Pattern_t *pattern = Buzzer_GetPattern(buzzer_current_pattern);

    if (pattern == NULL)
    {
        Buzzer_StopPattern();
        return;
    }

    while ((buzzer_current_pattern != BUZZER_PATTERN_NONE) &&
           ((int32_t)(now_tick - buzzer_step_deadline) >= 0))
    {
        buzzer_current_step++;

        if (buzzer_current_step >= pattern->step_count)
        {
            if (pattern->repeat)
            {
                buzzer_current_step = 0U;
                buzzer_step_deadline = now_tick + pattern->steps_ms[0];
            }
            else
            {
                Buzzer_StopPattern();
            }
            return;
        }

        buzzer_step_deadline += pattern->steps_ms[buzzer_current_step];
    }
}

static void Buzzer_Service(void)
{
    const Buzzer_Pattern_t *pattern;
    uint32_t now_tick;
    uint32_t remain_ms;
    uint32_t slice_ms;

    now_tick = HAL_GetTick();

    if (buzzer_suspended)
    {
        Buzzer_StopPattern();
        buzzer_low_battery_pending = 0U;
        buzzer_gps_rx_pending = 0U;
        osDelay(10U);
        return;
    }

    if (buzzer_alarm_active)
    {
        if (buzzer_current_pattern != BUZZER_PATTERN_ALARM)
        {
            Buzzer_StartPattern(BUZZER_PATTERN_ALARM, now_tick);
        }
    }
    else
    {
        if (buzzer_current_pattern == BUZZER_PATTERN_ALARM)
        {
            Buzzer_StopPattern();
        }

        if (buzzer_current_pattern == BUZZER_PATTERN_NONE)
        {
            if (buzzer_low_battery_pending)
            {
                buzzer_low_battery_pending = 0U;
                Buzzer_StartPattern(BUZZER_PATTERN_LOW_BATTERY, now_tick);
            }
            else if (buzzer_gps_rx_pending)
            {
                buzzer_gps_rx_pending = 0U;
                Buzzer_StartPattern(BUZZER_PATTERN_GPS_RX, now_tick);
            }
        }
    }

    if (buzzer_current_pattern == BUZZER_PATTERN_NONE)
    {
        Buzzer_Off();
        osDelay(10U);
        return;
    }

    Buzzer_AdvancePattern(now_tick);
    if (buzzer_current_pattern == BUZZER_PATTERN_NONE)
    {
        Buzzer_Off();
        osDelay(10U);
        return;
    }

    pattern = Buzzer_GetPattern(buzzer_current_pattern);
    if (pattern == NULL)
    {
        Buzzer_StopPattern();
        osDelay(10U);
        return;
    }

    remain_ms = buzzer_step_deadline - now_tick;
    slice_ms = remain_ms;
    if (slice_ms > BUZZER_SERVICE_SLICE_MS)
    {
        slice_ms = BUZZER_SERVICE_SLICE_MS;
    }

    if ((buzzer_current_step & 0x01U) == 0U)
    {
        Buzzer_OutputTone(pattern->tone_hz, slice_ms);
    }
    else
    {
        Buzzer_Off();
        osDelay(slice_ms);
    }
}

void Buzzer_Init(void)
{
//    Buzzer_GPIO_Init();
//    Buzzer_DWT_Init();
//    Buzzer_StopPattern();
//    Buzzer_Beep(BUZZER_CARRIER_FREQ_HZ, 80U);
//    Buzzer_DelayUs(40000U);
//    Buzzer_Beep(BUZZER_CARRIER_FREQ_HZ, 80U);
	
    Buzzer_GPIO_Init();
    Buzzer_DWT_Init();
    Buzzer_StopPattern();

}

void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
}

void Buzzer_Toggle(void)
{
    HAL_GPIO_TogglePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}

void Buzzer_Beep(uint32_t frequency, uint32_t duration_ms)
{
    uint32_t half_period_us;
    uint32_t cycles;
    uint32_t i;

    if ((frequency == 0U) || (duration_ms == 0U))
    {
        return;
    }

    half_period_us = 500000U / frequency;
    cycles = (frequency * duration_ms) / 1000U;

    for (i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
        Buzzer_DelayUs(half_period_us);
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
        Buzzer_DelayUs(half_period_us);
    }
}

void Buzzer_Suspend(void)
{
    buzzer_suspended = 1U;
    buzzer_alarm_active = 0U;
    Buzzer_StopPattern();
}

void Buzzer_Resume(void)
{
    buzzer_suspended = 0U;
}

void Buzzer_SetAlarmActive(uint8_t active)
{
    buzzer_alarm_active = active ? 1U : 0U;

    if (buzzer_alarm_active)
    {
        buzzer_low_battery_pending = 0U;
        buzzer_gps_rx_pending = 0U;
    }
}

void Buzzer_RequestReminder(Buzzer_Reminder_t type)
{
    if (buzzer_suspended || buzzer_alarm_active)
    {
        return;
    }

    switch (type)
    {
    case BUZZER_REMINDER_LOW_BATTERY:
        buzzer_low_battery_pending = 1U;
        break;

    case BUZZER_REMINDER_GPS_RX:
        buzzer_gps_rx_pending = 1U;
        break;

    default:
        break;
    }
}

void buzzer_task(void *argument)
{
    (void)argument;

#if BUZZER_HW_TEST_MODE
    for (;;)
    {
        Buzzer_Beep(BUZZER_CARRIER_FREQ_HZ, 500U);
        osDelay(500U);
    }
#else
    for (;;)
    {
        Buzzer_Service();
    }
#endif
}

#endif
