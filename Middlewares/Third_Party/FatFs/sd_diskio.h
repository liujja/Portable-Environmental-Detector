/**
  ******************************************************************************
  * @file    FatFs/FatFs_uSD_Standalone/Inc/sd_diskio.h
  * @author  MCD Application Team
  * @brief   Header for sd_diskio.c module
  ******************************************************************************
  */

#ifndef __SD_DISKIO_H
#define __SD_DISKIO_H

#include "stm32h7xx_hal.h"
#include <stdbool.h>

#define SD_TIMEOUT             SDMMC_DATATIMEOUT

#define SD_TOTAL_SIZE_BYTE(__Handle__)  (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>0)
#define SD_TOTAL_SIZE_KB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>10)
#define SD_TOTAL_SIZE_MB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>20)
#define SD_TOTAL_SIZE_GB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>30)

extern SD_HandleTypeDef        hsd1;
extern HAL_SD_CardInfoTypeDef  g_sd_card_info_handle;
extern volatile uint8_t g_sd_ready;
extern uint32_t g_sd_block_num;
extern uint16_t g_sd_block_size;

uint8_t sd_init(bool ShowMessage);
uint8_t get_sd_card_state(void);
uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);
uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);

#endif
