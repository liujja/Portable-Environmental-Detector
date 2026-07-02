#include <stdio.h>
#include <string.h>
#include "sd_diskio.h"
#include "sdmmc.h"

HAL_SD_CardInfoTypeDef g_sd_card_info_handle;

static volatile bool SD_Read_Done = false;
static volatile bool SD_Write_Done = false;

volatile uint8_t g_sd_ready = 0;
uint32_t g_sd_block_num = 0;
uint16_t g_sd_block_size = 512;

#if 0
uint8_t sd_init(bool ShowMessage)
{
	uint8_t SD_Error;
	memset(&hsd1, 0, sizeof(hsd1));
	memset(&g_sd_card_info_handle, 0, sizeof(g_sd_card_info_handle));

	hsd1.Instance = SDMMC1;
	hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
	hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
	hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
	hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd1.Init.ClockDiv = 10;

	HAL_SD_DeInit(&hsd1);
	SD_Error = HAL_SD_Init(&hsd1);
	if (SD_Error != HAL_OK)
	{
		return 1;
	}

	if (HAL_SD_ConfigSpeedBusOperation(&hsd1, SDMMC_SPEED_MODE_HIGH) == HAL_OK)
	{
		if (ShowMessage)
		{
			printf("SD card switched to high speed mode\r\n");
		}
	}

	HAL_SD_GetCardInfo(&hsd1, &g_sd_card_info_handle);
	g_sd_block_num = g_sd_card_info_handle.BlockNbr;
	g_sd_block_size = 512;
	g_sd_ready = 1;

	return 0;
}

uint8_t get_sd_card_state(void)
{
	if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)
		return 0;
	return 1;
}

uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
	uint32_t timeout;

	timeout = HAL_GetTick();
	while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
	{
		if (HAL_GetTick() - timeout > 2000)
			return HAL_TIMEOUT;
	}

	HAL_StatusTypeDef sta;

	sta = HAL_SD_ReadBlocks(
			&hsd1,
			(uint8_t *)pbuf,
			saddr,
			cnt,
			5000);

	if (sta != HAL_OK)
	{
		printf("SD Read Fail, sta=%d, addr=%lu, cnt=%lu, ErrorCode=0x%08lX, STA=0x%08lX, CLKCR=0x%08lX, DCTRL=0x%08lX\r\n",
		       sta,
		       saddr,
		       cnt,
		       hsd1.ErrorCode,
		       SDMMC1->STA,
		       SDMMC1->CLKCR,
		       SDMMC1->DCTRL);
		return 1;
	}

	return 0;
}

uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
	uint32_t timeout;
	HAL_StatusTypeDef sta;

	SCB_CleanDCache();

	timeout = HAL_GetTick();
	while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
	{
		if (HAL_GetTick() - timeout > 2000)
			return 1;
	}

	sta = HAL_SD_WriteBlocks(&hsd1, pbuf, saddr, cnt, 5000);

	if (sta != HAL_OK)
	{
		printf("SD Write Fail, sta=%d, addr=%lu, cnt=%lu, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
		       sta, saddr, cnt, hsd1.ErrorCode, SDMMC1->STA);
		return 1;
	}

	timeout = HAL_GetTick();
	while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
	{
		if (HAL_GetTick() - timeout > 5000)
		{
			printf("SD wait timeout after write, state=%d, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
			       HAL_SD_GetCardState(&hsd1),
			       hsd1.ErrorCode,
			       SDMMC1->STA);
			return 1;
		}
	}

	return 0;
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
	SD_Read_Done = true;
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
	SD_Write_Done = true;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
	SD_Read_Done = true;
	SD_Write_Done = true;
}
#endif

static void sd_mark_not_ready(void)
{
	g_sd_ready = 0;
	g_sd_block_num = 0;
	g_sd_block_size = 512;
	memset(&g_sd_card_info_handle, 0, sizeof(g_sd_card_info_handle));
}

static uint8_t sd_wait_transfer(uint32_t timeout_ms)
{
	uint32_t start = HAL_GetTick();

	while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
	{
		if ((HAL_GetTick() - start) > timeout_ms)
		{
			return 0;
		}
	}

	return 1;
}

static uint8_t sd_read_one_block(uint8_t *aligned_buf, uint32_t block_addr)
{
	HAL_StatusTypeDef sta;

	if (!sd_wait_transfer(2000))
	{
		printf("SD read wait timeout before read\r\n");
		sd_mark_not_ready();
		return 1;
	}

	SD_Read_Done = false;
	SD_Write_Done = false;

	sta = HAL_SD_ReadBlocks_DMA(&hsd1, aligned_buf, block_addr, 1);
	if (sta != HAL_OK)
	{
		printf("SD Read Fail, sta=%d, addr=%lu, cnt=1, ErrorCode=0x%08lX, STA=0x%08lX, CLKCR=0x%08lX, DCTRL=0x%08lX\r\n",
		       sta, block_addr, hsd1.ErrorCode, SDMMC1->STA, SDMMC1->CLKCR, SDMMC1->DCTRL);
		sd_mark_not_ready();
		return 1;
	}

	{
		uint32_t start = HAL_GetTick();
		while (!SD_Read_Done)
		{
			if ((HAL_GetTick() - start) > 5000U)
			{
				printf("SD DMA read callback timeout, addr=%lu, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
				       block_addr, hsd1.ErrorCode, SDMMC1->STA);
				sd_mark_not_ready();
				return 1;
			}
		}
	}

	if (!sd_wait_transfer(5000))
	{
		printf("SD read wait timeout after read, addr=%lu, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
		       block_addr, hsd1.ErrorCode, SDMMC1->STA);
		sd_mark_not_ready();
		return 1;
	}

	return 0;
}

static uint8_t sd_write_one_block(uint8_t *aligned_buf, uint32_t block_addr)
{
	HAL_StatusTypeDef sta;

	if (!sd_wait_transfer(2000))
	{
		printf("SD write wait timeout before write\r\n");
		sd_mark_not_ready();
		return 1;
	}

	SD_Read_Done = false;
	SD_Write_Done = false;

	sta = HAL_SD_WriteBlocks_DMA(&hsd1, aligned_buf, block_addr, 1);
	if (sta != HAL_OK)
	{
		printf("SD Write Fail, sta=%d, addr=%lu, cnt=1, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
		       sta, block_addr, hsd1.ErrorCode, SDMMC1->STA);
		sd_mark_not_ready();
		return 1;
	}

	{
		uint32_t start = HAL_GetTick();
		while (!SD_Write_Done)
		{
			if ((HAL_GetTick() - start) > 5000U)
			{
				printf("SD DMA write callback timeout, addr=%lu, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
				       block_addr, hsd1.ErrorCode, SDMMC1->STA);
				sd_mark_not_ready();
				return 1;
			}
		}
	}

	if (!sd_wait_transfer(5000))
	{
		printf("SD wait timeout after write, addr=%lu, ErrorCode=0x%08lX, STA=0x%08lX\r\n",
		       block_addr, hsd1.ErrorCode, SDMMC1->STA);
		sd_mark_not_ready();
		return 1;
	}

	return 0;
}

uint8_t sd_init(bool ShowMessage)
{
	HAL_StatusTypeDef sta;

	sd_mark_not_ready();
	memset(&hsd1, 0, sizeof(hsd1));

	hsd1.Instance = SDMMC1;
	hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
	hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
	hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
	hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd1.Init.ClockDiv = 120;

	HAL_SD_DeInit(&hsd1);

	sta = HAL_SD_Init(&hsd1);
	if (sta != HAL_OK)
	{
		printf("HAL_SD_Init failed, err=0x%08lX\r\n", hsd1.ErrorCode);
		return 1;
	}

	if (!sd_wait_transfer(1000))
	{
		printf("SD wait transfer timeout after init\r\n");
		sd_mark_not_ready();
		return 1;
	}

	HAL_SD_GetCardInfo(&hsd1, &g_sd_card_info_handle);
	g_sd_block_num = g_sd_card_info_handle.LogBlockNbr;
	g_sd_block_size = g_sd_card_info_handle.LogBlockSize;
	g_sd_ready = 1;

	if (ShowMessage)
	{
		printf("SD card init in 1-bit slow mode\r\n");
	}

	return 0;
}

uint8_t get_sd_card_state(void)
{
	if (!g_sd_ready)
		return 1;

	return (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) ? 0 : 1;
}

uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
	uint32_t i;
	__ALIGNED(32) static uint8_t sector_buf[512];

	if ((pbuf == NULL) || (cnt == 0) || (!g_sd_ready))
		return 1;

	for (i = 0; i < cnt; i++)
	{
		if (sd_read_one_block(sector_buf, saddr + i) != 0)
			return 1;

		memcpy(pbuf + (i * 512U), sector_buf, 512U);
	}

	return 0;
}

uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
	uint32_t i;
	__ALIGNED(32) static uint8_t sector_buf[512];

	if ((pbuf == NULL) || (cnt == 0) || (!g_sd_ready))
		return 1;

	for (i = 0; i < cnt; i++)
	{
		memcpy(sector_buf, pbuf + (i * 512U), 512U);

		if (sd_write_one_block(sector_buf, saddr + i) != 0)
			return 1;
	}

	return 0;
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
	(void)hsd;
	SD_Read_Done = true;
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
	(void)hsd;
	SD_Write_Done = true;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
	(void)hsd;
	SD_Read_Done = true;
	SD_Write_Done = true;
}
