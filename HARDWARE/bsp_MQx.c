#include "bsp_MQx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "usart.h"



#if ENABLE_BSP_MQX


/* ========================================================================
 * Global sensor values (PPM) ！ updated by sensor_task, read by GUI
 * ======================================================================== */
float g_mq2_ppm = 0.0f;
float g_mq4_ppm = 0.0f;

/* ========================================================================
 * MQx Hardware Initialization ！ MQ-2(PF11,CH2) + MQ-4(PA6,CH3) on ADC1
 * ======================================================================== */
static void MQx_HW_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStruct = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    /* Enable clocks */
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC12_CLK_ENABLE();

    /* MQ-2: PF11 ！ Analog input */
    GPIO_InitStruct.Pin  = MQ2_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MQ2_GPIO_PORT, &GPIO_InitStruct);

    /* MQ-4: PA6 ！ Analog input */
    GPIO_InitStruct.Pin  = MQ4_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MQ4_GPIO_PORT, &GPIO_InitStruct);

    /* ADC1 common init */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_16B;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hadc1.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.LeftBitShift          = ADC_LEFTBITSHIFT_NONE;
    hadc1.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        printf("MQx ADC1 Init Error");
    }

    /* Default channel config */
    sConfig.Rank                 = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime         = ADC_SAMPLETIME_64CYCLES_5;
    sConfig.SingleDiff           = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber         = ADC_OFFSET_NONE;
    sConfig.Offset               = 0;
    sConfig.OffsetSignedSaturation = DISABLE;
    sConfig.Channel              = MQ2_ADC_CHANNEL;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        printf("MQx ADC CH2 Config Error");
    }

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        printf("MQx ADC Calib Error");
    }
}

/* ========================================================================
 * MQx_ReadChannel ！ read average voltage (mV) from specified ADC channel
 * ======================================================================== */
float MQx_ReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t sum = 0;
    uint32_t val;
    int i;

    sConfig.Channel      = channel;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    sConfig.OffsetSignedSaturation = DISABLE;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    for (i = 0; i < MQx_BUFFER_SIZE; i++)
    {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            val = HAL_ADC_GetValue(&hadc1);
            sum += val;
        }
        HAL_ADC_Stop(&hadc1);
    }

    return ((float)sum / (float)MQx_BUFFER_SIZE) *
           (MQ_ADC_VREF * 1000.0f) / MQ_ADC_MAX;
}


float MQx_AdcMvToSensorMv(float adc_mv)
{
    if (adc_mv < 0.0f)
    {
        adc_mv = 0.0f;
    }

    return adc_mv * MQ_ADC_DIV_RATIO;  
}

float MQx_CalcRs(float adc_mv)
{
    float vout;

    vout = MQx_AdcMvToSensorMv(adc_mv) / 1000.0f;

    if (vout < 0.001f)
    {
        vout = 0.001f;
    }

    if (vout > (MQ_SENSOR_VCC - 0.001f))
    {
        vout = MQ_SENSOR_VCC - 0.001f;
    }

    return MQ_RL * (MQ_SENSOR_VCC - vout) / vout;
}

float MQx_CalcR0(float adc_mv, float clean_air_factor)
{
    float rs;

    if (clean_air_factor < 0.1f)
    {
        clean_air_factor = 0.1f;
    }

    rs = MQx_CalcRs(adc_mv);
    return rs / clean_air_factor;
}

/* ========================================================================
 * MQx_VoltageToPPM ！ convert sensor voltage to gas concentration (ppm)
 *
 * Step 1:  Rs = RL * (Vcc - Vout) / Vout               (kΩ)
 * Step 2:  ratio = Rs / R0_cal                          (unitless)
 * Step 3:  ppm  = PARA * ratio^PARB                      (ppm)
 *
 * Calibration: Run sensor 24h in clean air. Measure Vout_air.
 *   Rs_air = RL * (Vcc - Vout_air) / Vout_air
 *   R0_cal = Rs_air / clean_air_factor
 * Then replace MQx_R0_CAL with your calibrated R0_cal value.
 * ======================================================================== */
float MQx_VoltageToPPM(float adc_mv, float r0_cal, float para, float parb)
{
    float rs;
    float ratio;
    float ppm;

    if (r0_cal < 0.001f)
    {
        return 0.0f;
    }

    rs = MQx_CalcRs(adc_mv);
    ratio = rs / r0_cal;
    ppm = para * powf(ratio, parb);

    if ((ppm != ppm) || (ppm < 0.0f))
    {
        ppm = 0.0f;
    }

    if (ppm > 99999.0f)
    {
        ppm = 99999.0f;
    }

    return ppm;
}

/* ========================================================================
 * Task control
 * ======================================================================== */
static volatile uint8_t mqx_suspended = 0;

void MQx_Init(void)
{
    printf("MQx Init MQ-2(PF11) + MQ-4(PA6)...");
    MQx_HW_Init();
    printf("MQx Init Complete");
}

void MQx_Suspend(void) { mqx_suspended = 1; }
void MQx_Resume(void)  { mqx_suspended = 0; }

#endif /* ENABLE_BSP_MQX */




