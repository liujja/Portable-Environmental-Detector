#include "sys_task.h"
#include "main.h"
#include "bsp_buzzer.h"
#include "bsp_ewm201.h"
#include "bsp_mlx90640.h"
#include "NRF24L01.h"
#include "bsp_sht30.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32h7xx_hal.h"
#include "SPL06.h"
#include "usart.h"
#include "UBLOX.h"
#include "bsp_lcd.h"
#include "bsp_MQx.h"
#include "ugui.h"
#include "TF_Card.h"
#include "key.h"
#include "storage_manager.h"
#include "cmsis_os2.h"
#include "adc.h"
#include <stdio.h>
#include <stdarg.h>

#define BATTERY_ADC_FULL_SCALE     65535.0f
#define BATTERY_ADC_REF_VOLTAGE    3.3f
#define BATTERY_DIVIDER_RATIO      2.0f
#define BATTERY_CHG_DIVIDER_RATIO  ((10.0f + 4.7f) / 4.7f)
#define BATTERY_EMPTY_VOLTAGE      3.30f
#define BATTERY_FULL_VOLTAGE       4.20f
#define BATTERY_CHG_ACTIVE_THRESH  3.80f
#define MQ2_ALARM_PPM              300.0f
#define MQ4_ALARM_PPM              1000.0f
#define RADIATION_ALARM_USVH       1.00f
#define AMBIENT_TEMP_ALARM_C       45.0f
#define IR_TEMP_ALARM_C            50.0f
#define LOW_BATTERY_ALARM_PERCENT  15U
#define LOW_BATTERY_BEEP_INTERVAL  30000U
#define BUZZER_ALARM_DURATION_MS   10000U

float g_battery_voltage = 0.0f;
uint8_t g_battery_percent = 0;
float g_charge_voltage = 0.0f;
uint8_t g_charge_active = 0;
float g_mq2_mv = 0.0f;
float g_mq4_mv = 0.0f;

static uint8_t Battery_ReadSample(float *voltage,
                                  float *charge_voltage,
                                  uint8_t *percent,
                                  uint8_t *charge_active)
{
    uint32_t raw_bat;
    uint32_t raw_chg;
    float v_bat_pin;
    float v_chg_pin;
    float v_bat;
    float v_chg;
    float v_display;
    float pct;

    if ((voltage == NULL) || (charge_voltage == NULL) ||
        (percent == NULL) || (charge_active == NULL))
    {
        return 0;
    }

    if (HAL_ADC_Start(&hadc3) != HAL_OK)
    {
        return 0;
    }

    if (HAL_ADC_PollForConversion(&hadc3, 10) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc3);
        return 0;
    }

    raw_bat = HAL_ADC_GetValue(&hadc3);

    if (HAL_ADC_PollForConversion(&hadc3, 10) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc3);
        return 0;
    }

    raw_chg = HAL_ADC_GetValue(&hadc3);
    HAL_ADC_Stop(&hadc3);

    v_bat_pin = ((float)raw_bat / BATTERY_ADC_FULL_SCALE) * BATTERY_ADC_REF_VOLTAGE;
    v_chg_pin = ((float)raw_chg / BATTERY_ADC_FULL_SCALE) * BATTERY_ADC_REF_VOLTAGE;

    v_bat = v_bat_pin * BATTERY_DIVIDER_RATIO;
    v_chg = v_chg_pin * BATTERY_CHG_DIVIDER_RATIO;

    *charge_active = (v_chg >= BATTERY_CHG_ACTIVE_THRESH) ? 1U : 0U;
    v_display = (*charge_active) ? v_chg : v_bat;

    pct = ((v_display - BATTERY_EMPTY_VOLTAGE) /
          (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) * 100.0f;

    if (pct < 0.0f)
    {
        pct = 0.0f;
    }
    else if (pct > 100.0f)
    {
        pct = 100.0f;
    }

    *voltage = v_display;
    *charge_voltage = v_chg;
    *percent = (uint8_t)(pct + 0.5f);
    return 1;
}

static float Sensor_GetMaxIrTemp(void)
{
#if ENABLE_BSP_MLX90640
    float max_t = -1000.0f;
    uint8_t found = 0U;
    int i;

    for (i = 0; i < 768; i++)
    {
        float v = mlx90640_temperatures[i];

        if ((v > -40.0f) && (v < 300.0f))
        {
            if (!found || (v > max_t))
            {
                max_t = v;
            }
            found = 1U;
        }
    }

    if (found)
    {
        return max_t;
    }
#endif

    return -1000.0f;
}

#include "bsp_geiger.h"


void display_task(void *pvParameters);

void sensor_task(void *pvParameters);

void key_task(void *pvParameters);

#if ENABLE_BSP_MLX90640
void mlx90640_task(void *pvParameters);
#endif

static int log_buf_append(char *buf, size_t buf_size, int len, const char *fmt, ...)
{
    int written;
    size_t remain;
    va_list args;

    if ((buf == NULL) || (buf_size == 0U))
    {
        return 0;
    }

    if (len < 0)
    {
        len = 0;
    }

    if ((size_t)len >= buf_size)
    {
        buf[buf_size - 1U] = '\0';
        return (int)(buf_size - 1U);
    }

    remain = buf_size - (size_t)len;

    va_start(args, fmt);
    written = vsnprintf(buf + len, remain, fmt, args);
    va_end(args);

    if (written < 0)
    {
        buf[len] = '\0';
        return len;
    }

    if ((size_t)written >= remain)
    {
        return (int)(buf_size - 1U);
    }

    return len + written;
}


void bsp_init(void)
{
   

#if ENABLE_BSP_8080_LCD
    LCD_Initial();

#endif


#if ENABLE_BSP_SHT30
    SHT_Init();

#endif

#if ENABLE_BSP_MLX90640
    MLX90640_Init();
   
#endif

#if ENABLE_BSP_METHANE
    Methane_Init();

#endif

#if ENABLE_BSP_MQX
    MQx_Init();
  
#endif

#if ENABLE_BSP_GEIGER
    Geiger_Init();
   
#endif

#if ENABLE_BSP_BUZZER
    Buzzer_Init();

#endif

#if ENABLE_BSP_NRF24L01
    NRF24L01_Init(1);  /* RX mode */

#endif

#if ENABLE_BSP_EWM201
    EWM201_Init();
  
#endif

#if ENABLE_BSP_KEY
    KEY_Init();

#endif

#if ENABLE_BSP_NEO_M8P
    GPS_Init();
	
#endif



}




/* ========================================================================
 * sensor_task — general sensor data acquisition (always compiled)
 * ======================================================================== */

void sensor_task(void *argument)
{
    static uint32_t tick = 0;
    static uint8_t prev_hazard_alarm = 0U;
    static uint32_t hazard_alarm_until_tick = 0U;
    static uint32_t last_low_battery_beep_tick = 0U;
    static uint32_t last_gps_rx_tick = 0U;
    float mq2_mv = 0.0f;
    float mq4_mv = 0.0f;
    float battery_voltage;
    float charge_voltage;
    uint8_t battery_percent;
    uint8_t charge_active;
    float max_ir_temp = -1000.0f;
    float radiation_usvh = 0.0f;
    uint8_t mq2_alarm = 0U;
    uint8_t mq4_alarm = 0U;
    uint8_t radiation_alarm = 0U;
    uint8_t ambient_temp_alarm = 0U;
    uint8_t ir_temp_alarm = 0U;
    uint8_t hazard_alarm = 0U;
    uint8_t buzzer_alarm_active = 0U;
    uint32_t now_tick = 0U;

#if ENABLE_BSP_SPL06
    uint8_t  spl06_ready = 0;
    uint32_t spl06_retry_tick = 0;

    Baro.Valid = 0;
    spl06_ready = SPL06_Init();
    spl06_retry_tick = HAL_GetTick();
#endif
	
    for (;;)
    {
        /* ----- 1. SHT30 temperature & humidity ----- */
#if ENABLE_BSP_SHT30
        SHT30_read_result();
#endif

        /* ----- 2. SPL06 barometer ----- */
#if ENABLE_BSP_SPL06
          if (!spl06_ready)
        {
            Baro.Valid = 0;

            if ((HAL_GetTick() - spl06_retry_tick) >= 1000U)
            {
                spl06_retry_tick = HAL_GetTick();
                spl06_ready = SPL06_Init();
            }
        }
        else
        {
            SPL06_Task();
        }
#endif

        /* ----- 3. MQ-2 + MQ-4 → PPM ----- */
#if ENABLE_BSP_MQX
				
					float mq2_rs;
					float mq2_r0_suggest;
					float mq2_ratio;
					float mq4_rs;
					float mq4_r0_suggest;
					float mq4_ratio;

				mq2_mv = MQx_ReadChannel(MQ2_ADC_CHANNEL);
				g_mq2_mv = mq2_mv;
				g_mq2_ppm = MQx_VoltageToPPM(mq2_mv, MQ2_R0_CAL, MQ2_PARA, MQ2_PARB);
				mq2_rs = MQx_CalcRs(mq2_mv);
				mq2_r0_suggest = MQx_CalcR0(mq2_mv, MQ2_CLEAN_AIR_FACTOR);
				mq2_ratio = mq2_rs / MQ2_R0_CAL;

				mq4_mv = MQx_ReadChannel(MQ4_ADC_CHANNEL);
				g_mq4_mv = mq4_mv;
				g_mq4_ppm = MQx_VoltageToPPM(mq4_mv, MQ4_R0_CAL, MQ4_PARA, MQ4_PARB);
				mq4_rs = MQx_CalcRs(mq4_mv);
				mq4_r0_suggest = MQx_CalcR0(mq4_mv, MQ4_CLEAN_AIR_FACTOR);
				mq4_ratio = mq4_rs / MQ4_R0_CAL;


            if (tick % 100 == 0)
            {
					printf("MQ2 adc=%.1fmV ao=%.1fmV Rs=%.2fk R0=%.2fk ratio=%.4f ppm=%.2f\r\n",
								 mq2_mv, MQx_AdcMvToSensorMv(mq2_mv),
								 mq2_rs, MQ2_R0_CAL, mq2_ratio, g_mq2_ppm);

					printf("MQ4 adc=%.1fmV ao=%.1fmV Rs=%.2fk R0=%.2fk ratio=%.4f ppm=%.2f\r\n",
								 mq4_mv, MQx_AdcMvToSensorMv(mq4_mv),
								 mq4_rs, MQ4_R0_CAL, mq4_ratio, g_mq4_ppm);

					printf("MQ clean-air R0 suggest: MQ2=%.2fk MQ4=%.2fk\r\n",
								 mq2_r0_suggest, mq4_r0_suggest);
            }
#endif

        /* ----- 4. GPS (parsed asynchronously in USART1 ISR) ----- */
#if ENABLE_BSP_NEO_M8P
        /* GPS data is updated via GPS_Process_Byte() in USART1 interrupt.
           gps_position holds the latest PVT solution. */
        if (tick % 25 == 0)
        {
            printf("GPS diag: rx=%lu nmea=%lu pvt=%lu last=%lums fix=%u valid=%u sats=%u time=%u\r\n",
                   gps_rx_byte_count,
                   gps_nmea_sentence_count,
                   gps_ubx_pvt_count,
                   gps_last_rx_tick_ms,
                   gps_position.fix_type,
                   gps_position.data_valid,
                   gps_position.satellites,
                   gps_position.time_valid);
        }
#endif

        /* ----- 5. Battery voltage ----- */
        if (Battery_ReadSample(&battery_voltage, &charge_voltage,
                               &battery_percent, &charge_active))
        {
            g_battery_voltage = battery_voltage;
            g_battery_percent = battery_percent;
            g_charge_voltage = charge_voltage;
            g_charge_active = charge_active;
        }

        /* ----- 6. Geiger counter ----- */
#if ENABLE_BSP_GEIGER
        Geiger_Update();
#endif

        /* ----- 7. Buzzer alert / reminder dispatch ----- */
        now_tick = HAL_GetTick();
        mq2_alarm = 0U;
        mq4_alarm = 0U;
        radiation_alarm = 0U;
        ambient_temp_alarm = 0U;
        ir_temp_alarm = 0U;
        hazard_alarm = 0U;

#if ENABLE_BSP_MQX
        if (g_mq2_ppm >= MQ2_ALARM_PPM)
        {
            mq2_alarm = 1U;
        }

        if (g_mq4_ppm >= MQ4_ALARM_PPM)
        {
            mq4_alarm = 1U;
        }
#endif

#if ENABLE_BSP_GEIGER
        radiation_usvh = Geiger_GetuSv();
        if (radiation_usvh >= RADIATION_ALARM_USVH)
        {
            radiation_alarm = 1U;
        }
#endif

#if ENABLE_BSP_SHT30
        if (Th30BufTypedef.TH30_temp >= AMBIENT_TEMP_ALARM_C)
        {
            ambient_temp_alarm = 1U;
        }
#endif

#if ENABLE_BSP_MLX90640
        max_ir_temp = Sensor_GetMaxIrTemp();
        if (max_ir_temp >= IR_TEMP_ALARM_C)
        {
            ir_temp_alarm = 1U;
        }
#endif

        if (mq2_alarm || mq4_alarm || radiation_alarm ||
            ambient_temp_alarm || ir_temp_alarm)
        {
            hazard_alarm = 1U;
        }

#if ENABLE_BSP_BUZZER
        if (hazard_alarm != prev_hazard_alarm)
        {
            printf("Buzzer hazard %s: MQ2=%u MQ4=%u RAD=%u TEMP=%u IR=%u\r\n",
                   hazard_alarm ? "ON" : "OFF",
                   mq2_alarm,
                   mq4_alarm,
                   radiation_alarm,
                   ambient_temp_alarm,
                   ir_temp_alarm);

            if (hazard_alarm)
            {
                hazard_alarm_until_tick = now_tick + BUZZER_ALARM_DURATION_MS;
            }

            prev_hazard_alarm = hazard_alarm;
        }

        buzzer_alarm_active = 0U;
        if ((hazard_alarm_until_tick != 0U) &&
            ((int32_t)(hazard_alarm_until_tick - now_tick) > 0))
        {
            buzzer_alarm_active = 1U;
        }
        else
        {
            hazard_alarm_until_tick = 0U;
        }

        Buzzer_SetAlarmActive(buzzer_alarm_active);

        if (g_nrf24_rx_gps.frame_valid &&
            (g_nrf24_rx_gps.tick_ms != 0U) &&
            (g_nrf24_rx_gps.tick_ms != last_gps_rx_tick))
        {
            last_gps_rx_tick = g_nrf24_rx_gps.tick_ms;

            if (!buzzer_alarm_active)
            {
                Buzzer_RequestReminder(BUZZER_REMINDER_GPS_RX);
            }
        }

        if (g_charge_active || (g_battery_percent > LOW_BATTERY_ALARM_PERCENT) ||
            (g_battery_voltage <= 0.1f))
        {
            last_low_battery_beep_tick = 0U;
        }
        else if (!buzzer_alarm_active)
        {
            if ((last_low_battery_beep_tick == 0U) ||
                ((now_tick - last_low_battery_beep_tick) >= LOW_BATTERY_BEEP_INTERVAL))
            {
                last_low_battery_beep_tick = now_tick;
                Buzzer_RequestReminder(BUZZER_REMINDER_LOW_BATTERY);
            }
        }
#endif

        /* ----- 8. TF Card data logging (every 5s) ----- */
#if ENABLE_BSP_TF_CARD && ENABLE_LOCAL_TF_LOG_SUPPORT
        {
            static uint32_t last_log_tick = 0;
            if (tick - last_log_tick >= 50)  /* 5 seconds at 200ms period */
            {
                last_log_tick = tick;
                char log_buf[256];
                int len = 0;

                log_buf[0] = '\0';

#if ENABLE_BSP_SHT30
                len = log_buf_append(log_buf, sizeof(log_buf), len,
                    "temp=%.1f,hum=%.0f,",
                    Th30BufTypedef.TH30_temp, Th30BufTypedef.TH30_hum);
#endif
#if ENABLE_BSP_SPL06
                len = log_buf_append(log_buf, sizeof(log_buf), len,
                    "Pressure_kpa=%.1fkpa,Alt_meter=%.1fm,",
                    Baro.Pressure_kpa, Baro.Alt_meter);
#endif
#if ENABLE_BSP_MQX
                len = log_buf_append(log_buf, sizeof(log_buf), len,
                    "MQ2=%.0fpmm,MQ4=%.0fpmm,", g_mq2_ppm, g_mq4_ppm);
#endif
#if ENABLE_BSP_NEO_M8P
                len = log_buf_append(log_buf, sizeof(log_buf), len,
                    "GPS_lat=%.7f,GPS_lon=%.7f,GPS_alt=%.2f,%d,",
                    gps_position.lat / 10000000.0,
                    gps_position.lon / 10000000.0,
                    gps_position.alt / 1000.0f,
                    gps_position.satellites);
#endif
                len = log_buf_append(log_buf, sizeof(log_buf), len,
                    "Battery_V=%.2f,Battery_Pct=%u,Charge_V=%.2f,Charge_Active=%u,",
                    g_battery_voltage, g_battery_percent,
                    g_charge_voltage, g_charge_active);
#if ENABLE_BSP_GEIGER
                len = log_buf_append(log_buf, sizeof(log_buf), len,
                    "Geiger_CPM=%lu,Geiger_uSv=%.4f,",
                    Geiger_GetCPM(), Geiger_GetuSv());
#endif
                len = log_buf_append(log_buf, sizeof(log_buf), len, "\r\n");
                if (osMutexAcquire(tfFsMutexHandle, osWaitForever) == osOK)
                {
                    TF_Card_AppendFileLen("sensor.csv", log_buf, (uint32_t)len);
                    osMutexRelease(tfFsMutexHandle);
                }
								
            }
        }
#endif

        tick++;
        osDelay(pdMS_TO_TICKS(200));
    }
}


