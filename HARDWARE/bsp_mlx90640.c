#include "bsp_mlx90640.h"
#include "sys_task.h"

#if ENABLE_BSP_MLX90640

#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "cmsis_os2.h"
/* ========================================================================
 * MLX90640 I2C Driver Interface (required by MLX90640_API.c)
 * ======================================================================== */
#define MLX_REINIT_DELAY_MS     1000U
#define MLX_FRAME_DELAY_MS      50U
#define MLX_FAIL_REINIT_COUNT   5U
#define MLX_LOG_INTERVAL_MS     5000U

int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data)
{
    int i;

    if (HAL_I2C_Mem_Read(MLX_I2C_HANDLE, slaveAddr, startAddress, I2C_MEMADD_SIZE_16BIT,
                         (uint8_t *)data, nMemAddressRead * 2, 1000) != HAL_OK)
    {
        return -1;
    }

    /* Byte-swap: MLX90640 sends big-endian, STM32 is little-endian */
    for (i = 0; i < nMemAddressRead; i++)
    {
        uint8_t *p = (uint8_t *)&data[i];
        uint8_t tmp = p[0];
        p[0] = p[1];
        p[1] = tmp;
    }

    return 0;
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    uint8_t cmd[4];
    cmd[0] = writeAddress >> 8;
    cmd[1] = writeAddress & 0xFF;
    cmd[2] = data >> 8;
    cmd[3] = data & 0xFF;

    if (HAL_I2C_Master_Transmit(MLX_I2C_HANDLE, slaveAddr, cmd, 4, 100) != HAL_OK)
    {
        return -1;
    }
    return 0;
}

void MLX90640_I2CInit(void)
{
    /* Already handled by MX_I2C1_Init + GPIO init */
}

int MLX90640_I2CGeneralReset(void)
{
    return 0;
}

void MLX90640_I2CFreqSet(int freq)
{
    /* Not implemented - using fixed timing */
    (void)freq;
}

/* ========================================================================
 * BSP Init & GPIO
 * ======================================================================== */

static void MLX90640_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = MLX90640_I2C_SCL_PIN | MLX90640_I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(MLX90640_I2C_GPIO_PORT, &GPIO_InitStruct);
    MX_I2C1_Init();
}

void MLX90640_Init(void)
{
  
    MLX90640_GPIO_Init();
    MLX90640_SetResolution(MLX90640_ADDR, 0x02); /* 18-bit, match EEPROM calibration */
    MLX90640_SetRefreshRate(MLX90640_ADDR, 0x03); /* 4Hz */
    MLX90640_SetChessMode(MLX90640_ADDR);        /* Enable chess mode for better visuals */
    
}

/* ========================================================================
 * Task Suspend / Resume
 * ======================================================================== */

static volatile uint8_t mlx90640_suspended = 0;

void MLX90640_Suspend(void)
{
  
    mlx90640_suspended = 1;
}

void MLX90640_Resume(void)
{
    
    mlx90640_suspended = 0;
}

/* ========================================================================
 * Global Data
 * ======================================================================== */
float mlx90640_temperatures[768];

/* ========================================================================
 * FreeRTOS Task
 * ======================================================================== */

void mlx90640_task(void *argument)
{
    static uint16_t eeData[832];
    static uint16_t frame[834];
    static paramsMLX90640 params;
    int status;
    int subPage;
    uint32_t fail_count = 0;
    uint32_t last_log_time = 0;
    uint32_t now;
    float ta;
    float tr;

    (void)argument;
	
	    for (;;)
    {
        if (mlx90640_suspended)
        {
            osDelay(100);
            continue;
        }

    /* Step 1: Read EEPROM calibration data and extract parameters */
    
    status = MLX90640_DumpEE(MLX90640_ADDR, eeData);
    if (status != 0)
    {
       
            osDelay(MLX_REINIT_DELAY_MS);
            continue;
    }
    

    status = MLX90640_ExtractParameters(eeData, &params);
    if (status < 0)
    {
			      osDelay(MLX_REINIT_DELAY_MS);
            continue;
        
        /* Non-fatal for some warnings, continue */
    }
    /* Diagnostic: dump key calibration parameters */
        MLX90640_GetFrameData(MLX90640_ADDR, frame);
        osDelay(300);
        MLX90640_GetFrameData(MLX90640_ADDR, frame);
        osDelay(300);

        fail_count = 0;

   
        if (!mlx90640_suspended)
        {
            /* Read one frame */
            subPage = MLX90640_GetFrameData(MLX90640_ADDR, frame);
            if (subPage >= 0)
            {
                ta = MLX90640_GetTa(frame, &params);
                tr = ta - 8.0f;
                MLX90640_CalculateTo(frame, &params, 0.95f, tr, mlx90640_temperatures);

                /* Print every 2 seconds */
                fail_count = 0;
						  	now = osKernelGetTickCount();
                if ((now - last_log_time) >= MLX_LOG_INTERVAL_MS)
                {
                   last_log_time = now;
                }
            }
            else
            {
                fail_count++;
							  now = osKernelGetTickCount();
							if ((now - last_log_time) >= MLX_LOG_INTERVAL_MS)
                {
                    last_log_time = now;
                    printf("MLX90640_GetFrameData failed: %d, fail_count=%lu\r\n",
                           subPage, (unsigned long)fail_count);
                }
								
								if (fail_count >= MLX_FAIL_REINIT_COUNT)
                {
                    printf("MLX90640 reinit...\r\n");
                    break;
                }
								
            }
						osDelay(MLX_FRAME_DELAY_MS);
        }

        osDelay(MLX_FRAME_DELAY_MS);
    }
}
		

#endif /* ENABLE_BSP_MLX90640 */
