#ifndef __SYS_TASK_H
#define __SYS_TASK_H

#include <stdint.h>

#define ENABLE_BSP_BUZZER   1       // Buzzer Module
#define SYSTEM_SUPPORT_OS   1
#define ENABLE_BSP_NEO_M8P  1
#define ENABLE_BSP_EWM201   1
#define ENABLE_BSP_MLX90640 1
#define ENABLE_BSP_SHT30    1
#define ENABLE_BSP_SPL06    1
#define ENABLE_BSP_MQX      1
#define ENABLE_BSP_KEY      1
#define ENABLE_BSP_TF_CARD  1
#define ENABLE_BSP_8080_LCD 1
#define ENABLE_BSP_UGUI     1
#define ENABLE_BSP_NRF24L01 1         
#define ENABLE_BSP_GEIGER   1
#define ENABLE_USB_MSC_SUPPORT      1         //支持按键切换到USB_MSC
#define ENABLE_LOCAL_TF_LOG_SUPPORT      1    //支持MCU本地TF记录
void bsp_init(void);

/*
 * 默认上电模式：
 * 0：默认本地 TF 记录
 * 1：默认 USB MSC
 *
 * 通过按键进入 USB MSC，所以这里为 0。
 */
#define STORAGE_DEFAULT_USB_MSC      0

#endif
