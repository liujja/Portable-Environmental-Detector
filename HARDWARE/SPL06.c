#include "SPL06.h"
#include <math.h>
#include <stdint.h>
#include "main.h"
#include "shared_bus_lock.h"
#include "usart.h"

SPL06 spl06;
SPL06_Calib_Param spl06_calib_param;

LPF LP_Baro;
Kalman KM_BaroPressure;
Baro_Data Baro;
uint32_t timestamp_data = 0;



static void SPL06_DumpRegs(const char *tag)
{
    uint8_t id;
    uint8_t meas_cfg;
    uint8_t prs_cfg;
    uint8_t tmp_cfg;
    uint8_t cfg_reg;

    id       = SPL06_ReadREG(SP06_ID);
    meas_cfg = SPL06_ReadREG(SP06_MEAS_CFG);
    prs_cfg  = SPL06_ReadREG(SP06_PSR_CFG);
    tmp_cfg  = SPL06_ReadREG(SP06_TMP_CFG);
    cfg_reg  = SPL06_ReadREG(SP06_CFG_REG);

    printf("[SPL06][%s] ID=0x%02X MEAS_CFG=0x%02X PRS_CFG=0x%02X TMP_CFG=0x%02X CFG_REG=0x%02X\r\n",
           tag, id, meas_cfg, prs_cfg, tmp_cfg, cfg_reg);
}

static void SPL06_DumpCalib(void)
{
    printf("[SPL06][CAL] c0=%d c1=%d c00=%ld c10=%ld c01=%d c11=%d c20=%d c21=%d c30=%d\r\n",
           spl06_calib_param.c0,
           spl06_calib_param.c1,
           (long)spl06_calib_param.c00,
           (long)spl06_calib_param.c10,
           spl06_calib_param.c01,
           spl06_calib_param.c11,
           spl06_calib_param.c20,
           spl06_calib_param.c21,
           spl06_calib_param.c30);
}


void SPL06_delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for(i = 0; i < ms; i++)
    {
        for(j = 0; j < 20000; j++)
        {
            __NOP();
        }
    }
}

float LPF_Filter(LPF *lpf, float input)
{
    float dt = 1.0f / lpf->sample_rate;
    float RC = 1.0f / (2.0f * 3.1415926f * lpf->cutoff_freq);
    float alpha = dt / (RC + dt);
    lpf->output = alpha * input + (1 - alpha) * lpf->output;
    return lpf->output;
}

void LPF_SetCutoffFreq(LPF *lpf, float sample_rate, float cutoff_freq)
{
    lpf->sample_rate = sample_rate;
    lpf->cutoff_freq = cutoff_freq;
    lpf->output = 0;
}

void Init_KalmanFilter(Kalman *kf, float Q, float R)
{
    kf->Q = Q;
    kf->R = R;
    kf->P = 1;
    kf->K = 0;
    kf->X = 0;
}

float KalmanFilter(Kalman *kf, float input)
{
    kf->P = kf->P + kf->Q;
    
    kf->K = kf->P / (kf->P + kf->R);
    kf->X = kf->X + kf->K * (input - kf->X);
    kf->P = (1 - kf->K) * kf->P;
    
    return kf->X;
}

void SPL06_delay(void)
{
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
}

uint8_t SPL06_RW_Fast(uint8_t uchar)
{
    uint8_t bit_ctr;
    for (bit_ctr = 0; bit_ctr < 8; bit_ctr++)
    {
        if ((uchar & 0x80) == 0x00)
            SPL06_MOSI_RESET; // SPL06_MOSI=0
        else
            SPL06_MOSI_SET;   // SPL06_MOSI=1
        
        SPL06_SCK_RESET;     // SPL06_SCK=0
        uchar = uchar << 1;
        SPL06_delay();
        SPL06_SCK_SET;       // SPL06_SCK=1
        SPL06_delay();
        
        if (SPL06_MISO_READ)
            uchar |= 0x01;
    }
    return uchar;
}

uint8_t SPL06_ReadREG(uint8_t Reg)
{
    uint8_t value;
    SPL06_CS_RESET; // SPL06_CS=0
    SPL06_RW_Fast(Reg | 0x80);
    value = SPL06_RW_Fast(0xFF);
    SPL06_CS_SET;   // SPL06_CS=1
    SPL06_delay();
    return value;
}

void SPL06_WriteREG(uint8_t Reg, uint8_t value)
{
    SPL06_CS_RESET; // SPL06_CS=0
    SPL06_RW_Fast(Reg & 0x7F);
    SPL06_RW_Fast(value);
    SPL06_CS_SET;   // SPL06_CS=1
    SPL06_delay();
}

void spl06_start(uint8_t mode)
{
    SPL06_WriteREG(SP06_MEAS_CFG, mode);
}

void spl06_config_temperature(uint8_t rate, uint8_t oversampling)
{
    uint8_t data;
    data = (1 << 7) | rate | oversampling;
    
    if (oversampling > TMP_PRC_8)
    {
        uint8_t reg_data;
        reg_data = SPL06_ReadREG(SP06_CFG_REG);
        reg_data |= SPL06_CFG_T_SHIFT;
        SPL06_WriteREG(SP06_CFG_REG, reg_data);
    }
    
    switch (oversampling)
    {
        case TMP_PRC_1:
            spl06.i32kT = 524288;
            break;
        case TMP_PRC_2:
            spl06.i32kT = 1572864;
            break;
        case TMP_PRC_4:
            spl06.i32kT = 3670016;
            break;
        case TMP_PRC_8:
            spl06.i32kT = 7864320;
            break;
        case TMP_PRC_16:
            spl06.i32kT = 253952;
            break;
        case TMP_PRC_32:
            spl06.i32kT = 516096;
            break;
        case TMP_PRC_64:
            spl06.i32kT = 1040384;
            break;
        case TMP_PRC_128:
            spl06.i32kT = 2088960;
            break;
    }
    SPL06_WriteREG(SP06_TMP_CFG, data);
}

void spl06_config_pressure(uint8_t rate, uint8_t oversampling)
{
    uint8_t data;
    data = rate | oversampling;
    
    if (oversampling > PM_PRC_8)
    {
        uint8_t reg_data;
        reg_data = SPL06_ReadREG(SP06_CFG_REG);
        reg_data |= SPL06_CFG_P_SHIFT;
        SPL06_WriteREG(SP06_CFG_REG, reg_data);
    }
    
    switch (oversampling)
    {
        case PM_PRC_1:
            spl06.i32kP = 524288;
            break;
        case PM_PRC_2:
            spl06.i32kP = 1572864;
            break;
        case PM_PRC_4:
            spl06.i32kP = 3670016;
            break;
        case PM_PRC_8:
            spl06.i32kP = 7864320;
            break;
        case PM_PRC_16:
            spl06.i32kP = 253952;
            break;
        case PM_PRC_32:
            spl06.i32kP = 516096;
            break;
        case PM_PRC_64:
            spl06.i32kP = 1040384;
            break;
        case PM_PRC_128:
            spl06.i32kP = 2088960;
            break;
    }
    SPL06_WriteREG(SP06_PSR_CFG, data);
}

int32_t spl06_get_pressure_adc(void)
{
    uint8_t buf[3];
    int32_t adc;
    
    SPL06_CS_RESET; // SPL06_CS=0
    SPL06_RW_Fast(SP06_PSR_B2 | 0x80);
    buf[0] = SPL06_RW_Fast(0xFF);
    buf[1] = SPL06_RW_Fast(0xFF);
    buf[2] = SPL06_RW_Fast(0xFF);
    SPL06_CS_SET;  // SPL06_CS=1
    SPL06_delay();
    
    adc = (int32_t)buf[0] << 16 | (int32_t)buf[1] << 8 | (int32_t)buf[2];
    adc = (adc & 0x800000) ? (0xFF000000 | adc) : adc;
    return adc;
}

int32_t spl06_get_temperature_adc(void)
{
    uint8_t buf[3];
    int32_t adc;
    
    SPL06_CS_RESET; // SPL06_CS=0
    SPL06_RW_Fast(SP06_TMP_B2 | 0x80);
    buf[0] = SPL06_RW_Fast(0xFF);
    buf[1] = SPL06_RW_Fast(0xFF);
    buf[2] = SPL06_RW_Fast(0xFF);
    SPL06_CS_SET;   // SPL06_CS=1
    SPL06_delay();
    
    adc = (int32_t)buf[0] << 16 | (int32_t)buf[1] << 8 | (int32_t)buf[2];
    adc = (adc & 0x800000) ? (0xFF000000 | adc) : adc;
    return adc;
}

void spl06_get_calib_param(void)
{
    uint8_t h, m, l;
    
    h = SPL06_ReadREG(0x10);
    l = SPL06_ReadREG(0x11);
    spl06_calib_param.c0 = (int16_t)h << 4 | l >> 4;
    spl06_calib_param.c0 = (spl06_calib_param.c0 & 0x0800) ? (0xF000 | spl06_calib_param.c0) : spl06_calib_param.c0;
    
    h = SPL06_ReadREG(0x11);
    l = SPL06_ReadREG(0x12);
    spl06_calib_param.c1 = (int16_t)(h & 0x0F) << 8 | l;
    spl06_calib_param.c1 = (spl06_calib_param.c1 & 0x0800) ? (0xF000 | spl06_calib_param.c1) : spl06_calib_param.c1;
    
    h = SPL06_ReadREG(0x13);
    m = SPL06_ReadREG(0x14);
    l = SPL06_ReadREG(0x15);
    spl06_calib_param.c00 = (int32_t)h << 12 | (int32_t)m << 4 | (int32_t)l >> 4;
    spl06_calib_param.c00 = (spl06_calib_param.c00 & 0x080000) ? (0xFFF00000 | spl06_calib_param.c00) : spl06_calib_param.c00;
    
    h = SPL06_ReadREG(0x15);
    m = SPL06_ReadREG(0x16);
    l = SPL06_ReadREG(0x17);
    spl06_calib_param.c10 = (int32_t)h << 16 | (int32_t)m << 8 | l;
    spl06_calib_param.c10 = (spl06_calib_param.c10 & 0x080000) ? (0xFFF00000 | spl06_calib_param.c10) : spl06_calib_param.c10;
    
    h = SPL06_ReadREG(0x18);
    l = SPL06_ReadREG(0x19);
    spl06_calib_param.c01 = (int16_t)h << 8 | l;
    
    h = SPL06_ReadREG(0x1A);
    l = SPL06_ReadREG(0x1B);
    spl06_calib_param.c11 = (int16_t)h << 8 | l;
    
    h = SPL06_ReadREG(0x1C);
    l = SPL06_ReadREG(0x1D);
    spl06_calib_param.c20 = (int16_t)h << 8 | l;
    
    h = SPL06_ReadREG(0x1E);
    l = SPL06_ReadREG(0x1F);
    spl06_calib_param.c21 = (int16_t)h << 8 | l;
    
    h = SPL06_ReadREG(0x20);
    l = SPL06_ReadREG(0x21);
    spl06_calib_param.c30 = (int16_t)h << 8 | l;
}

void SPL06_Task(void)
{
    static uint32_t last_diag_tick = 0;

    int32_t raw_t_adc;
    int32_t raw_p_adc;
    float Praw_src = 0.0f, Traw_src = 0.0f;
    float qua2 = 0.0f, qua3 = 0.0f;
    float temp_c = 0.0f;
    float pressure_kpa = 0.0f;
    float alt_filtered = 0.0f;
    double Pressure = 0.0;
    float Alt = 0.0f;
    uint32_t now;

    Baro.Valid = 0;

    if (!SharedBus_Lock(20))
    {

        return;
    }

    SharedBus_PrepareForSPL06();

    if ((spl06.i32kT == 0) || (spl06.i32kP == 0))
    {

        goto exit;
    }

    raw_t_adc = spl06_get_temperature_adc();
    raw_p_adc = spl06_get_pressure_adc();

    Traw_src = raw_t_adc / (float)spl06.i32kT;
    Praw_src = raw_p_adc / (float)spl06.i32kP;

    now = HAL_GetTick();

    if (!(-1000.0f < Traw_src && Traw_src < 1000.0f))
    {
        if ((now - last_diag_tick) >= 1000U)
        {
            last_diag_tick = now;
 
            SPL06_DumpRegs("bad_traw");
        }
        goto exit;
    }

    if (!(-1000.0f < Praw_src && Praw_src < 1000.0f))
    {
        if ((now - last_diag_tick) >= 1000U)
        {
            last_diag_tick = now;
  
            SPL06_DumpRegs("bad_praw");
        }
        goto exit;
    }

    temp_c = 0.5f * spl06_calib_param.c0 + spl06_calib_param.c1 * Traw_src;

    qua2 = spl06_calib_param.c10 +
           Praw_src * (spl06_calib_param.c20 + Praw_src * spl06_calib_param.c30);

    qua3 = Traw_src * Praw_src *
           (spl06_calib_param.c11 + Praw_src * spl06_calib_param.c21);

    Pressure = spl06_calib_param.c00 +
               Praw_src * qua2 +
               Traw_src * spl06_calib_param.c01 +
               qua3;

    if (!(Pressure > 1000.0 && Pressure < 200000.0))
    {
        if ((now - last_diag_tick) >= 1000U)
        {
            last_diag_tick = now;
       
        }
        goto exit;
    }

    pressure_kpa = (float)(Pressure / 1000.0);
    if (!(pressure_kpa > 1.0f && pressure_kpa < 200.0f))
    {
        if ((now - last_diag_tick) >= 1000U)
        {
            last_diag_tick = now;
            
        }
        goto exit;
    }

    Alt = (float)(44330.0 * (1.0 - pow(Pressure / 101325.0, 0.190295)));
    if (!(Alt > -1000.0f && Alt < 20000.0f))
    {
        if ((now - last_diag_tick) >= 1000U)
        {
            last_diag_tick = now;
          
        }
        goto exit;
    }

    alt_filtered = LPF_Filter(&LP_Baro, Alt);
    if (!(alt_filtered > -1000.0f && alt_filtered < 20000.0f))
    {
        if ((now - last_diag_tick) >= 1000U)
        {
            last_diag_tick = now;
          
        }
        goto exit;
    }

    Baro.Temp = temp_c;
    Baro.Pressure_kpa = pressure_kpa;
    Baro.Pressure_kpa_Kalman = KalmanFilter(&KM_BaroPressure, pressure_kpa);
    Baro.Alt_meter = alt_filtered;
    Baro.timestamp = HAL_GetTick();
    Baro.Valid = 1;

exit:
    SharedBus_PrepareForSPL06();
    SharedBus_Unlock();
}

uint8_t SPL06_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint32_t timeout;
    uint8_t spl06_start_status;
    uint8_t ok = 0;
    uint8_t chip_id;

    printf("[SPL06] Init start\r\n");

    if (!SharedBus_Lock(50))
    {
        return 0;
    }

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    LPF_SetCutoffFreq(&LP_Baro, 100.0f, 30.0f);
    Init_KalmanFilter(&KM_BaroPressure, 1.0f, 40.0f);

    SharedBus_PrepareForSPL06();

    SPL06_DumpRegs("before_reset");

    SPL06_WriteREG(SP06_RESET, 0x89);
    SPL06_delay_ms(50);

    SPL06_DumpRegs("after_reset");

    timeout = 0;
    while (1)
    {
        spl06_start_status = SPL06_ReadREG(SP06_MEAS_CFG);
        if ((spl06_start_status & MEAS_COEF_RDY) == MEAS_COEF_RDY)
            break;

        timeout++;
        if (timeout >= 10000)
        {
            SPL06_DumpRegs("coef_timeout");
            goto exit;
        }
        vTaskDelay(1);
    }

    spl06_get_calib_param();
    SPL06_DumpCalib();

    timeout = 0;
    while (1)
    {
        spl06_start_status = SPL06_ReadREG(SP06_MEAS_CFG);
        if ((spl06_start_status & MEAS_SENSOR_RDY) == MEAS_SENSOR_RDY)
            break;

        timeout++;
        if (timeout >= 10000)
        {
        
            SPL06_DumpRegs("sensor_timeout");
            goto exit;
        }
        vTaskDelay(1);
    }

   

    chip_id = SPL06_ReadREG(SP06_ID);
    if (chip_id != 0x10)
    {

        SPL06_DumpRegs("bad_id");
        goto exit;
    }

    spl06_config_pressure(PM_RATE_128, PM_PRC_8);
    spl06_config_temperature(TMP_RATE_128, TMP_PRC_8);
    spl06_start(MEAS_CTRL_ContinuousPressTemp);

    SPL06_DumpRegs("after_config");
 

    ok = 1;

exit:
    SharedBus_PrepareForSPL06();
    SharedBus_Unlock();
    return ok;
}


