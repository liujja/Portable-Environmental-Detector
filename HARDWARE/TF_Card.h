#ifndef _TF_CARD_H
#define _TF_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sys_task.h"

/* SDMMC1 Pin Definitions (ALIENTEK Apollo H743 TF Card Interface) */
#define TF_SDMMC                        SDMMC1
#define TF_SDMMC_CLK_ENABLE()           __HAL_RCC_SDMMC1_CLK_ENABLE()
#define TF_SDMMC_CLK_DISABLE()          __HAL_RCC_SDMMC1_CLK_DISABLE()

#define TF_SDMMC_D0_PORT                GPIOC
#define TF_SDMMC_D0_PIN                 GPIO_PIN_8
#define TF_SDMMC_D0_AF                  GPIO_AF12_SDMMC1

#define TF_SDMMC_D1_PORT                GPIOC
#define TF_SDMMC_D1_PIN                 GPIO_PIN_9
#define TF_SDMMC_D1_AF                  GPIO_AF12_SDMMC1

#define TF_SDMMC_D2_PORT                GPIOC
#define TF_SDMMC_D2_PIN                 GPIO_PIN_10
#define TF_SDMMC_D2_AF                  GPIO_AF12_SDMMC1

#define TF_SDMMC_D3_PORT                GPIOC
#define TF_SDMMC_D3_PIN                 GPIO_PIN_11
#define TF_SDMMC_D3_AF                  GPIO_AF12_SDMMC1

#define TF_SDMMC_CK_PORT                GPIOC
#define TF_SDMMC_CK_PIN                 GPIO_PIN_12
#define TF_SDMMC_CK_AF                  GPIO_AF12_SDMMC1

#define TF_SDMMC_CMD_PORT               GPIOD
#define TF_SDMMC_CMD_PIN                GPIO_PIN_2
#define TF_SDMMC_CMD_AF                 GPIO_AF12_SDMMC1

#define TF_BLOCK_SIZE                   512

uint8_t TF_Card_Init(void);
uint8_t TF_Card_ReadBlock(uint32_t block_addr, uint8_t *buf);
uint8_t TF_Card_WriteBlock(uint32_t block_addr, const uint8_t *buf);
uint8_t TF_Card_ReadMultiBlocks(uint32_t block_addr, uint8_t *buf, uint32_t num_blocks);
uint8_t TF_Card_WriteMultiBlocks(uint32_t block_addr, const uint8_t *buf, uint32_t num_blocks);
void TF_Card_GetInfo(void);
uint32_t TF_Card_GetBlockCount(void);
uint8_t TF_Card_IsReady(void);

uint8_t TF_Card_WriteString(uint32_t sector_addr, const char *str);
uint8_t TF_Card_ReadString(uint32_t sector_addr, char *buf, uint16_t buf_size);
uint8_t TF_Card_AppendString(uint32_t start_sector, const char *str);
void TF_Card_EraseSectorRange(uint32_t start_sector, uint32_t end_sector);

uint8_t TF_Card_MountFS(void);
uint8_t TF_Card_UnmountFS(void);
uint8_t TF_Card_Format(void);
uint8_t TF_Card_CreateFile(const char *filename);
uint8_t TF_Card_WriteFile(const char *filename, const char *data);
uint8_t TF_Card_AppendFileLen(const char *filename, const char *data, uint32_t len);
uint8_t TF_Card_AppendFile(const char *filename, const char *data);
uint8_t TF_Card_ReadFile(const char *filename, char *buf, uint16_t buf_size);
uint32_t TF_Card_GetFileSize(const char *filename);
uint8_t TF_Card_DeleteFile(const char *filename);
uint8_t TF_Card_FileExists(const char *filename);
uint32_t TF_Card_GetFreeMB(void);
void TF_Card_ListFiles(void);

#ifdef __cplusplus
}
#endif

#endif

