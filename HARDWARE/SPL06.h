#ifndef _SPL06_H_
#define _SPL06_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sys_task.h"

// SPI引脚定义
#define SPL06_SCK_SET    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_SET)
#define SPL06_SCK_RESET  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_RESET)
#define SPL06_MOSI_SET   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_SET)
#define SPL06_MOSI_RESET HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_RESET)
#define SPL06_MISO_READ  HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_9)
#define SPL06_CS_SET     HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_SET)
#define SPL06_CS_RESET   HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_RESET)


// 气压测量速率(sample/sec),Background模式使用
#define PM_RATE_1          (0<<4)
#define PM_RATE_2          (1<<4)
#define PM_RATE_4          (2<<4)
#define PM_RATE_8          (3<<4)
#define PM_RATE_16         (4<<4)
#define PM_RATE_32         (5<<4)
#define PM_RATE_64         (6<<4)
#define PM_RATE_128        (7<<4)

// 气压过采样速率(times)
#define PM_PRC_1           0
#define PM_PRC_2           1
#define PM_PRC_4           2
#define PM_PRC_8           3
#define PM_PRC_16          4
#define PM_PRC_32          5
#define PM_PRC_64          6
#define PM_PRC_128         7

// 温度测量速率(sample/sec)
#define TMP_RATE_1         (0<<4)
#define TMP_RATE_2         (1<<4)
#define TMP_RATE_4         (2<<4)
#define TMP_RATE_8         (3<<4)
#define TMP_RATE_16        (4<<4)
#define TMP_RATE_32        (5<<4)
#define TMP_RATE_64        (6<<4)
#define TMP_RATE_128       (7<<4)

// 温度重采样速率(times)
#define TMP_PRC_1           0
#define TMP_PRC_2           1
#define TMP_PRC_4           2
#define TMP_PRC_8           3
#define TMP_PRC_16          4
#define TMP_PRC_32          5
#define TMP_PRC_64          6
#define TMP_PRC_128         7

// SPL06_MEAS_CFG
#define MEAS_COEF_RDY       0x80
#define MEAS_SENSOR_RDY     0x40
#define MEAS_TMP_RDY        0x20
#define MEAS_PRS_RDY        0x10

#define MEAS_CTRL_Standby               0x00
#define MEAS_CTRL_PressMeasure          0x01
#define MEAS_CTRL_TempMeasure           0x02
#define MEAS_CTRL_ContinuousPress       0x05
#define MEAS_CTRL_ContinuousTemp        0x06
#define MEAS_CTRL_ContinuousPressTemp   0x07

// FIFO_STS
#define SPL06_FIFO_FULL     0x02
#define SPL06_FIFO_EMPTY    0x01

// INT_STS
#define SPL06_INT_FIFO_FULL     0x04
#define SPL06_INT_TMP           0x02
#define SPL06_INT_PRS           0x01

// CFG_REG
#define SPL06_CFG_T_SHIFT   0x08
#define SPL06_CFG_P_SHIFT   0x04

#define SP06_PSR_B2     0x00
#define SP06_PSR_B1     0x01
#define SP06_PSR_B0     0x02
#define SP06_TMP_B2     0x03
#define SP06_TMP_B1     0x04
#define SP06_TMP_B0     0x05

#define SP06_PSR_CFG    0x06
#define SP06_TMP_CFG    0x07
#define SP06_MEAS_CFG   0x08
#define SP06_CFG_REG    0x09
#define SP06_INT_STS    0x0A
#define SP06_FIFO_STS   0x0B
#define SP06_RESET      0x0C
#define SP06_ID         0x0D
#define SP06_COEF       0x10

// 低通滤波器结构体
typedef struct {
    float cutoff_freq;
    float sample_rate;
    float output;
} LPF;

// 卡尔曼滤波器结构体
typedef struct {
    float Q;  // 过程噪声协方差
    float R;  // 测量噪声协方差
    float P;  // 估计误差协方差
    float K;  // 卡尔曼增益
    float X;  // 状态值
} Kalman;

// 气压计数据结构体
typedef struct {
    float Temp;
    float Pressure_kpa;
    float Pressure_kpa_Kalman;
    float Alt_meter;
    uint32_t timestamp;
    uint8_t Valid;
} Baro_Data;


typedef struct {
    int16_t c0;
    int16_t c1;
    int32_t c00;
    int32_t c10;
    int16_t c01;
    int16_t c11;
    int16_t c20;
    int16_t c21;
    int16_t c30;
} SPL06_Calib_Param;

typedef struct {
    uint8_t chip_id;
    int32_t i32rawPressure;
    int32_t i32rawTemperature;
    int32_t i32kP;
    int32_t i32kT;
} SPL06;

extern SPL06 spl06;
extern SPL06_Calib_Param spl06_calib_param;
extern LPF LP_Baro;
extern Kalman KM_BaroPressure;
extern Baro_Data Baro;
extern uint32_t timestamp_data;

void SPL06_delay(void);
void SPL06_delay_ms(uint32_t ms);
uint8_t SPL06_RW_Fast(uint8_t uchar);
uint8_t SPL06_ReadREG(uint8_t Reg);
void SPL06_WriteREG(uint8_t Reg, uint8_t value);
void spl06_start(uint8_t mode);
void spl06_config_temperature(uint8_t rate, uint8_t oversampling);
void spl06_config_pressure(uint8_t rate, uint8_t oversampling);
int32_t spl06_get_pressure_adc(void);
int32_t spl06_get_temperature_adc(void);
void spl06_get_calib_param(void);
void SPL06_Task(void);
uint8_t SPL06_Init(void);

float LPF_Filter(LPF *lpf, float input);
float KalmanFilter(Kalman *kf, float input);
void LPF_SetCutoffFreq(LPF *lpf, float sample_rate, float cutoff_freq);
void Init_KalmanFilter(Kalman *kf, float Q, float R);


#ifdef __cplusplus
}
#endif

#endif

