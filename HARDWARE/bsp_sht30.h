#ifndef __BSP_SHT30_H
#define __BSP_SHT30_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

// -------------------------------------------------------------
// SHT30 Hardware Configuration (Hardware I2C1)
// -------------------------------------------------------------
// Ensure hi2c1 is accessible (externed from i2c.c or main.c)
extern I2C_HandleTypeDef hi2c1;
#define SHT30_I2C_HANDLE &hi2c1

// -------------------------------------------------------------
// SHT30 Address
// -------------------------------------------------------------
// Note: SHT30 default 7-bit address is 0x44. HAL requires 8-bit shifted address.
#define SHT30_ADDRESS   (0x44 << 1)

// -------------------------------------------------------------
// SHT30 Data Structure
// -------------------------------------------------------------
typedef struct {
    uint16_t temp;
    uint16_t humi;
    float    TH30_temp;
    float    TH30_hum;
} __TH30BufTypedef;

extern __TH30BufTypedef Th30BufTypedef;

// -------------------------------------------------------------
// Function Prototypes
// -------------------------------------------------------------
void SHT_Init(void);
void SHT30_read_result(void);

#endif /* __BSP_SHT30_H */
