#ifndef __BSP_MLX90640_H
#define __BSP_MLX90640_H

#include "stm32h7xx_hal.h"
#include "MLX90640_API.h"

// I2C Interface Configuration
#define MLX90640_I2C_PORT           hi2c1
#define MLX90640_I2C_ADDR           0x33                 // Default 7-bit address
#define MLX90640_ADDR               (MLX90640_I2C_ADDR << 1) // 8-bit address for HAL
#define MLX_I2C_HANDLE              &MLX90640_I2C_PORT

#define MLX90640_I2C_GPIO_PORT      GPIOB
#define MLX90640_I2C_SCL_PIN        GPIO_PIN_6
#define MLX90640_I2C_SDA_PIN        GPIO_PIN_7

// BSP-Level Function Prototypes
void MLX90640_Init(void);

// Exported Data
extern float mlx90640_temperatures[768];

// Task Control API
void MLX90640_Suspend(void);
void MLX90640_Resume(void);
void mlx90640_task(void *argument);

#endif // __BSP_MLX90640_H
