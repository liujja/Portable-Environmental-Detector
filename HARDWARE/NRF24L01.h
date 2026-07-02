#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <string.h>
#include "UBLOX.h"


/* 引脚定义 */
#define NRF24L01_CE_PORT        GPIOD
#define NRF24L01_CE_PIN         GPIO_PIN_3
#define NRF24L01_IRQ_PORT       GPIOD
#define NRF24L01_IRQ_PIN        GPIO_PIN_4
#define NRF24L01_CSN_PORT       GPIOI
#define NRF24L01_CSN_PIN        GPIO_PIN_7

#define NRF24L01_SCK_PORT       GPIOG
#define NRF24L01_SCK_PIN        GPIO_PIN_11
#define NRF24L01_MISO_PORT      GPIOG
#define NRF24L01_MISO_PIN       GPIO_PIN_9
#define NRF24L01_MOSI_PORT      GPIOD
#define NRF24L01_MOSI_PIN       GPIO_PIN_7

/* 寄存器 */
#define NRF24_REG_CONFIG        0x00
#define NRF24_REG_EN_AA         0x01
#define NRF24_REG_EN_RXADDR     0x02
#define NRF24_REG_SETUP_AW      0x03
#define NRF24_REG_SETUP_RETR    0x04
#define NRF24_REG_RF_CH         0x05
#define NRF24_REG_RF_SETUP      0x06
#define NRF24_REG_STATUS        0x07
#define NRF24_REG_RX_ADDR_P0    0x0A
#define NRF24_REG_TX_ADDR       0x10
#define NRF24_REG_RX_PW_P0      0x11
#define NRF24_REG_FIFO_STATUS   0x17

#define NRF24_CMD_R_REGISTER    0x00
#define NRF24_CMD_W_REGISTER    0x20
#define NRF24_CMD_R_RX_PAYLOAD  0x61
#define NRF24_CMD_W_TX_PAYLOAD  0xA0
#define NRF24_CMD_FLUSH_TX      0xE1
#define NRF24_CMD_FLUSH_RX      0xE2
#define NRF24_CMD_NOP           0xFF

/* 地址：两个板子使用完全相同的地址（关闭自动应答，无需区分） */
#define NRF24_ADDRESS           ((uint8_t*)"Node1")

#define NRF24_PLOAD_WIDTH       16
#define NRF24_GPS_HEAD0         0xA5
#define NRF24_GPS_HEAD1         0x5A

/* 引脚控制宏 */
#define NRF24L01_CSN_HIGH()     HAL_GPIO_WritePin(NRF24L01_CSN_PORT, NRF24L01_CSN_PIN, GPIO_PIN_SET)
#define NRF24L01_CSN_LOW()      HAL_GPIO_WritePin(NRF24L01_CSN_PORT, NRF24L01_CSN_PIN, GPIO_PIN_RESET)
#define NRF24L01_CE_HIGH()      HAL_GPIO_WritePin(NRF24L01_CE_PORT, NRF24L01_CE_PIN, GPIO_PIN_SET)
#define NRF24L01_CE_LOW()       HAL_GPIO_WritePin(NRF24L01_CE_PORT, NRF24L01_CE_PIN, GPIO_PIN_RESET)
#define NRF24L01_IRQ_READ()     HAL_GPIO_ReadPin(NRF24L01_IRQ_PORT, NRF24L01_IRQ_PIN)

#define NRF24L01_SCK_HIGH()     HAL_GPIO_WritePin(NRF24L01_SCK_PORT, NRF24L01_SCK_PIN, GPIO_PIN_SET)
#define NRF24L01_SCK_LOW()      HAL_GPIO_WritePin(NRF24L01_SCK_PORT, NRF24L01_SCK_PIN, GPIO_PIN_RESET)
#define NRF24L01_MOSI_HIGH()    HAL_GPIO_WritePin(NRF24L01_MOSI_PORT, NRF24L01_MOSI_PIN, GPIO_PIN_SET)
#define NRF24L01_MOSI_LOW()     HAL_GPIO_WritePin(NRF24L01_MOSI_PORT, NRF24L01_MOSI_PIN, GPIO_PIN_RESET)
#define NRF24L01_MISO_READ()    HAL_GPIO_ReadPin(NRF24L01_MISO_PORT, NRF24L01_MISO_PIN)

/* 函数声明 */
void NRF24L01_Init(uint8_t mode);       // mode: 0=发送模式, 1=接收模式
void NRF24L01_SendPacket(uint8_t *data, uint8_t len);
uint8_t NRF24L01_ReceivePacket(uint8_t *data);
void NRF_Delay(void);
void NRF24_ReceiveGpsLocation(void);
uint8_t NRF24_SendGpsLocation(const GPS_Position_t *pos);
void NRF24L01_SetMode(uint8_t mode);   // 0=TX, 1=RX


typedef struct
{
    int32_t lat;
    int32_t lon;
    uint8_t hour;       /* Beijing time */
    uint8_t minute;
    uint8_t second;
    uint8_t data_valid;  /* 对端 GPS 是否有效 */
  	uint8_t frame_valid; /* 无线帧是否成功收到并校验通过 */
    uint32_t tick_ms;    /* local receive timestamp */
} NRF24_GPS_Rx_t;


extern volatile NRF24_GPS_Rx_t g_nrf24_rx_gps;

#endif

