/**
 ****************************************************************************************************
 * @file        key.c
 * @brief       6-key input driver ！ GPIO pull-up, active-low, 20ms debounce
 * @note        key_task() calls KEY_Scan() at 10ms intervals and feeds a ring
 *              buffer. Consumers call KEY_GetEvent() to retrieve buffered events.
 ****************************************************************************************************
 */

#include "key.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* ========================================================================
 * GPIO Pin maps
 * ======================================================================== */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} key_pin_t;

/* Index order: KEY_UP=0, KEY_DOWN=1, KEY_RETURN=2, KEY_RIGHT=3, KEY_ENTER=4, KEY_LEFT=5 */
static const key_pin_t key_pins[6] = {
    { GPIOB, GPIO_PIN_9  },   /* [0] KEY_UP     = PB9  */
    { GPIOI, GPIO_PIN_4  },   /* [1] KEY_DOWN   = PI4  */
    { GPIOI, GPIO_PIN_5  },   /* [2] KEY_RETURN = PI5  */
    { GPIOI, GPIO_PIN_6  },   /* [3] KEY_RIGHT  = PI6  */
    { GPIOG, GPIO_PIN_13 },   /* [4] KEY_ENTER  = PG13 */
    { GPIOG, GPIO_PIN_14 },   /* [5] KEY_LEFT   = PG14 */
};

/* ========================================================================
 * Debounce state
 * ======================================================================== */
static uint8_t key_last_state[6] = { 1,1,1,1,1,1 };    /* Last read level   */
static uint8_t key_debounce[6]  = { 0,0,0,0,0,0 };    /* Debounce counter  */
#define KEY_DEBOUNCE_MS   2   /* ~20ms (2 scans at 10ms interval) */

/* ========================================================================
 * KEY_Init ！ configure all key GPIOs as inputs with pull-up
 * ======================================================================== */
void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;
    int i;

    /* Enable clocks for all key ports */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    GPIO_Initure.Mode = GPIO_MODE_INPUT;
    GPIO_Initure.Pull = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_LOW;

    for (i = 0; i < 6; i++)
    {
        GPIO_Initure.Pin = key_pins[i].pin;
        HAL_GPIO_Init(key_pins[i].port, &GPIO_Initure);
    }
}

/* ========================================================================
 * KEY_Scan ！ edge-triggered scan with debounce
 * Returns   0..5 = key index on press (rising edge after debounce)
 *           0xFF = no key event
 * ======================================================================== */
uint8_t KEY_Scan(void)
{
    uint8_t i;
    uint8_t current;

    for (i = 0; i < 6; i++)
    {
        current = (uint8_t)HAL_GPIO_ReadPin(key_pins[i].port, key_pins[i].pin);

        if (current == KEY_PRESS)  /* Pressed (active-low) */
        {
            if (key_last_state[i] == 1)  /* Was released ★ now pressed */
            {
                key_debounce[i]++;
                if (key_debounce[i] >= KEY_DEBOUNCE_MS)
                {
                    key_debounce[i]  = 0;
                    key_last_state[i] = 0;  /* Mark as pressed */
                    return i;               /* Fire edge event */
                }
            }
            else
            {
                /* Held down ！ no edge */
                key_debounce[i] = 0;
            }
        }
        else  /* Released */
        {
            if (key_last_state[i] == 0)  /* Was pressed ★ now released */
            {
                key_debounce[i]++;
                if (key_debounce[i] >= KEY_DEBOUNCE_MS)
                {
                    key_debounce[i]  = 0;
                    key_last_state[i] = 1;  /* Mark as released */
                }
            }
            else
            {
                key_debounce[i] = 0;
            }
        }
    }

    return KEY_NONE;
}

/* ========================================================================
 * Ring buffer ！ SPSC lock-free, consumer = GUI task, producer = key_task
 * ======================================================================== */
#define KEY_BUF_SIZE 8
static uint8_t key_buf[KEY_BUF_SIZE];
static volatile uint8_t key_buf_head = 0;   /* Producer index  */
static volatile uint8_t key_buf_tail = 0;   /* Consumer index  */

/* ========================================================================
 * KEY_GetEvent ！ pop one event from the ring buffer
 * Returns   key index (0..5) or KEY_NONE if buffer is empty
 * ======================================================================== */
uint8_t KEY_GetEvent(void)
{
    uint8_t key;
    if (key_buf_head == key_buf_tail)
        return KEY_NONE;
    key = key_buf[key_buf_tail];
    key_buf_tail = (uint8_t)((key_buf_tail + 1) % KEY_BUF_SIZE);
    return key;
}

/* ========================================================================
 * KEY_Feed ！ push one event into the ring buffer (internal, from ISR-safe context)
 * ======================================================================== */
static void KEY_Feed(uint8_t key)
{
    uint8_t next = (uint8_t)((key_buf_head + 1) % KEY_BUF_SIZE);
    if (next != key_buf_tail)   /* Not full */
    {
        key_buf[key_buf_head] = key;
        key_buf_head = next;
    }
    /* else: buffer overflow ！ oldest event dropped */
}

/* ========================================================================
 * key_task ！ high-priority task, scans keys at 10ms interval
 * ======================================================================== */
void key_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        uint8_t key = KEY_Scan();
        if (key != KEY_NONE)
        {
            KEY_Feed(key);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
