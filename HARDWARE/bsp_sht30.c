#include "bsp_sht30.h"
#include "sys_task.h"

#if ENABLE_BSP_SHT30

__TH30BufTypedef Th30BufTypedef;

// -------------------------------------------------------------
// SHT30 Hardware I2C Initialization
// -------------------------------------------------------------
void SHT_Init(void)
{
    // Hardware I2C initialization is typically handled in main.c (MX_I2C1_Init).
    // The GPIOs (PB6, PB7) for I2C1 are already configured to Alternate Function Open-Drain.
    // If you need any specific sensor configuration upon startup, place it here.
}

// -------------------------------------------------------------
// SHT30 Application Layer (Hardware I2C)
// -------------------------------------------------------------
void SHT30_read_result(void)
{
    uint16_t temp, humi;
    float temperature = 0, humidity = 0;
    uint8_t cmd[2] = {0x2C, 0x06}; // High Repeatability Measurement
    uint8_t rx_buffer[6];
    
    // Send Read Command using Hardware I2C
    if (HAL_I2C_Master_Transmit(SHT30_I2C_HANDLE, SHT30_ADDRESS, cmd, 2, 100) != HAL_OK)
    {
        return; // Transmit failed
    }
    
    // Wait for measurement to complete (usually 15ms is enough)
    HAL_Delay(50);
    
    // Read Data using Hardware I2C
    if (HAL_I2C_Master_Receive(SHT30_I2C_HANDLE, SHT30_ADDRESS, rx_buffer, 6, 100) == HAL_OK)
    {
        temp = ((rx_buffer[0] << 8) | rx_buffer[1]);
        humi = ((rx_buffer[3] << 8) | rx_buffer[4]);
        
        // Calculate actual values
        temperature = (175.0f * (float)temp / 65535.0f - 45.0f);
        humidity = (100.0f * (float)humi / 65535.0f);
        
        // Filter wrong values
        if((temperature >= -10) && (temperature <= 125) && (humidity >= 0) && (humidity <= 100))
        {
            Th30BufTypedef.TH30_hum  = humidity;
            Th30BufTypedef.TH30_temp = temperature;
            Th30BufTypedef.temp      = temp;
            Th30BufTypedef.humi      = humi;
        }
    }
}

#endif // ENABLE_BSP_SHT30

