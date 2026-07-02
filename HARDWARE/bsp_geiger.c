#include "bsp_geiger.h"
#include "sys_task.h"
#include "usart.h"
#include <stdio.h>

#if ENABLE_BSP_GEIGER

// Circular buffer for 60 seconds of data
#define BUFFER_SIZE 60

static volatile uint32_t current_pulse_count = 0;
static uint16_t counts_buffer[BUFFER_SIZE];
static uint8_t buffer_index = 0;
static uint32_t total_cpm = 0;
static float dose_rate_uSv = 0.0f;
static uint32_t last_update_tick = 0;

static void Geiger_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOA Clock
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure PA8 as Geiger pulse input interrupt
    GPIO_InitStruct.Pin = GEIGER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GEIGER_GPIO_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(GEIGER_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GEIGER_IRQn);
}

void Geiger_Init(void) {
    printf("Geiger Initializing Geiger Counter...\r\n");
    Geiger_GPIO_Init();
    
    // Clear buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
        counts_buffer[i] = 0;
    }
    current_pulse_count = 0;
    total_cpm = 0;
    dose_rate_uSv = 0.0f;
    buffer_index = 0;
    last_update_tick = HAL_GetTick();
    printf("Geiger Initialization Complete\r\n");
}

// Interrupt Callback function
void Geiger_Callback(void) {
    current_pulse_count++;
    // BSP_LOG("Geiger", "Pulse Detected"); // Uncomment for detailed interrupt logging (Caution: Blocking)
}

// Call this function periodically in main loop or 1Hz timer
void Geiger_Update(void) {
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_update_tick >= 1000) { // Every 1 second
        last_update_tick = current_tick;
        
        // Remove old count from total
        total_cpm -= counts_buffer[buffer_index];
        
        // Add new count to buffer and total
        counts_buffer[buffer_index] = current_pulse_count;
        total_cpm += current_pulse_count;
        
        // Update index
        buffer_index = (buffer_index + 1) % BUFFER_SIZE;
        
        // Reset current count for next second
        current_pulse_count = 0;
        
        // Calculate dose rate
        dose_rate_uSv = (float)total_cpm * GEIGER_CPM_TO_USV;
        
//        printf("Geiger Update: CPM=%lu, Dose=%.4f uSv/h\r\n", total_cpm, dose_rate_uSv);
    }
}

uint32_t Geiger_GetCPM(void) {
    return total_cpm;
}

float Geiger_GetuSv(void) {
    return dose_rate_uSv;
}

uint32_t Geiger_GetPulseCount(void) {
    return current_pulse_count;
}

// Task suspend flag
static volatile uint8_t geiger_suspended = 0;

void Geiger_Suspend(void) {
    printf("Geiger Task Suspended\r\n");
    geiger_suspended = 1;
}

void Geiger_Resume(void) {
    printf("Geiger Task Resumed\r\n");
    geiger_suspended = 0;
}

/**
 * @brief Geiger Driver Task
 * @note  Recommended Priority: osPriorityBelowNormal
 * @note  Recommended Stack Size: 256 words (1024 bytes)
 * @param argument: Not used
 */
void geiger_task(void *argument) {
    printf("Geiger Task Started\r\n");
    
    for(;;) {
        if (!geiger_suspended) {
            // Task Logic: Update CPM every 1s
            Geiger_Update();
        }
        
        // Periodic Delay (100ms for responsiveness, but Update handles 1s)
        vTaskDelay(100);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GEIGER_GPIO_PIN)
    {
        Geiger_Callback();
    }
}

#endif // ENABLE_BSP_GEIGER
