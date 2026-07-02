#ifndef __BSP_MQX_H
#define __BSP_MQX_H

#include "sys_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "adc.h"
/* ========================================================================
 * MQ Sensor Configuration (ADC1)
 *  - MQ-2: PF11 = ADC1_INP2 = Channel 2  (flammable gas / smoke)
 *  - MQ-4: PA6  = ADC12_INP3 = Channel 3 (methane / natural gas)
 * ======================================================================== */

#define MQ2_GPIO_PORT   GPIOF
#define MQ2_GPIO_PIN    GPIO_PIN_11
#define MQ2_ADC_CHANNEL  ADC_CHANNEL_2

#define MQ4_GPIO_PORT   GPIOA
#define MQ4_GPIO_PIN    GPIO_PIN_6
#define MQ4_ADC_CHANNEL  ADC_CHANNEL_3

/* ADC Handle (global from CubeMX / main.c) */
extern ADC_HandleTypeDef hadc1;

/* Sample buffer size */
#define MQx_BUFFER_SIZE  10

/* ========================================================================
 * Gas concentration conversion (Voltage → Rs/R0 → PPM)
 *
 * Formula:  Rs = RL * (Vcc - Vout) / Vout
 *           ratio = Rs / R0_cal
 *           ppm  = PARA * ratio^PARB
 *
 * Calibration: R0_cal = Rs_clean_air / CLEAN_AIR_FACTOR
 *   - Run sensor in clean air for 24h, measure average Vout,
 *     then compute: R0_cal = RL*(Vcc-Vout)/Vout / CLEAN_AIR_FACTOR
 *   - Or tune R0_CAL directly until clean-air ppm reads near zero
 * ======================================================================== */

/* ---- Circuit constants ---- */
#define MQ_RL                (1.0f)     /* Load resistor (kΩ)      */
#define MQ_SENSOR_VCC        (5.0f)       /* MQ module supply voltage (V)       */
#define MQ_ADC_VREF          (3.3f)       /* MCU ADC reference voltage (V)      */
#define MQ_ADC_MAX           (65535.0f)  /* 16-bit ADC full scale    */
#define MQ_ADC_DIV_RATIO     (1.0f)       /* AO->ADC divider ratio, no divider=1 */

/* ---- MQ-2 (广谱) ----  Range: 300~10000 ppm ---- */
#define MQ2_CLEAN_AIR_FACTOR (9.83f)     /* Rs/R0 in clean air       */
#define MQ2_R0_CAL           (17.13f)      /* R0 calibrated value (kΩ) 1号板子*/
//#define MQ2_R0_CAL           (379.65f)      /* R0 calibrated value (kΩ) 2号板子*/
#define MQ2_PARA             (1160.0f)   /* ppm scale factor         */
#define MQ2_PARB             (-1.83f)    /* log-log exponent         */

/* ---- MQ-4 (甲烷) ----  Range: 300~10000 ppm ---- */
#define MQ4_CLEAN_AIR_FACTOR (4.4f)      /* Rs/R0 in clean air       */
#define MQ4_R0_CAL           (29.11f)      /* R0 calibrated value (kΩ) 1号板子*/
//#define MQ4_R0_CAL           (34.72f)      /* R0 calibrated value (kΩ) 2号板子*/
#define MQ4_PARA             (1060.0f)   /* ppm scale factor         */
#define MQ4_PARB             (-2.18f)    /* log-log exponent         */

/* ========================================================================
 * Global sensor values — updated by sensor_task, read by GUI
 * ======================================================================== */
extern float g_mq2_ppm;
extern float g_mq4_ppm;

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */
void     MQx_Init(void);
float    MQx_ReadChannel(uint32_t channel);     /* ADC average → mV       */
float    MQx_AdcMvToSensorMv(float adc_mv);      /* 传感器AO电压，单位 mV */
float    MQx_CalcRs(float adc_mv);               /* 传感器Rs，单位 kΩ     */
float    MQx_CalcR0(float adc_mv, float clean_air_factor);
float    MQx_VoltageToPPM(float mv,             /* mV → PPM conversion     */
                          float r0_cal,
                          float para, float parb);

void     MQx_Suspend(void);
void     MQx_Resume(void);

#endif /* __BSP_MQX_H */
