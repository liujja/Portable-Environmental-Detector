#include "TF_Card.h"
#include "usart.h"
#include "ff.h"
#include "sd_diskio.h"
#include <string.h>
#include <stdio.h>

static FATFS tf_fatfs;
static uint8_t fs_mounted = 0;


static uint8_t TF_Card_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    if (!g_sd_ready)
    {
        return 0;
    }

    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - start) > timeout_ms)
        {
            return 0;
        }
    }

    return 1;
}

uint8_t TF_Card_Init(void)
{
	if (sd_init(1) != 0)
	{
		printf("TF Card Init Failed!\r\n");
		return 0;
	}

	printf("TF Card Init OK, BlockCount=%lu\r\n",
	       g_sd_card_info_handle.LogBlockNbr);

	return 1;
}

uint8_t TF_Card_ReadBlock(uint32_t block_addr, uint8_t *buf)
{
	if (buf == NULL)
		return 0;

	if (!TF_Card_WaitReady(500))
		return 0;

	return (sd_read_disk(buf, block_addr, 1) == 0) ? 1 : 0;
}

uint8_t TF_Card_WriteBlock(uint32_t block_addr, const uint8_t *buf)
{
	if (buf == NULL)
		return 0;

	if (!TF_Card_WaitReady(500))
		return 0;

	return (sd_write_disk((uint8_t *)buf, block_addr, 1) == 0) ? 1 : 0;
}

uint8_t TF_Card_ReadMultiBlocks(uint32_t block_addr, uint8_t *buf, uint32_t num_blocks)
{
	if (buf == NULL || num_blocks == 0)
		return 0;

	if (!TF_Card_WaitReady(500))
		return 0;

	return (sd_read_disk(buf, block_addr, num_blocks) == 0) ? 1 : 0;
}

uint8_t TF_Card_WriteMultiBlocks(uint32_t block_addr, const uint8_t *buf, uint32_t num_blocks)
{
	if (buf == NULL || num_blocks == 0)
		return 0;

	if (!TF_Card_WaitReady(500))
		return 0;

	return (sd_write_disk((uint8_t *)buf, block_addr, num_blocks) == 0) ? 1 : 0;
}

void TF_Card_GetInfo(void)
{
	uint32_t card_capacity_mb;
	HAL_SD_CardInfoTypeDef *info = &g_sd_card_info_handle;

	if (!TF_Card_WaitReady(500))
	{
		printf("TF Card not ready\r\n");
		return;
	}

	card_capacity_mb = (uint32_t)(info->LogBlockNbr >> 11);

	printf("===== TF Card Info =====\r\n");
	printf("CardType: %lu\r\n", info->CardType);
	printf("CardVersion: %lu\r\n", info->CardVersion);
	printf("Class: %lu\r\n", info->Class);
	printf("BlockNbr: %lu\r\n", info->LogBlockNbr);
	printf("BlockSize: %lu\r\n", info->LogBlockSize);
	printf("Capacity: %lu MB\r\n", card_capacity_mb);
	printf("========================\r\n");
}

uint32_t TF_Card_GetBlockCount(void)
{	
	return g_sd_block_num;
}

uint8_t TF_Card_IsReady(void)
{
    return (g_sd_ready && (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)) ? 1U : 0U;
}

uint8_t TF_Card_WriteString(uint32_t sector_addr, const char *str)
{
	uint8_t buf[TF_BLOCK_SIZE];
	uint16_t len;

	if (str == NULL)
		return 0;

	len = (uint16_t)strlen(str);
	if (len > TF_BLOCK_SIZE - 2)
		len = TF_BLOCK_SIZE - 2;

	memset(buf, 0, TF_BLOCK_SIZE);
	buf[0] = (uint8_t)(len & 0xFF);
	buf[1] = (uint8_t)((len >> 8) & 0xFF);
	memcpy(&buf[2], str, len);

	return TF_Card_WriteBlock(sector_addr, buf);
}

uint8_t TF_Card_ReadString(uint32_t sector_addr, char *buf, uint16_t buf_size)
{
	uint8_t sector_buf[TF_BLOCK_SIZE];
	uint16_t len;

	if (buf == NULL || buf_size == 0)
		return 0;

	if (TF_Card_ReadBlock(sector_addr, sector_buf) == 0)
		return 0;

	len = (uint16_t)sector_buf[0] | ((uint16_t)sector_buf[1] << 8);

	if (len == 0)
	{
		buf[0] = '\0';
		return 1;
	}

	if (len > TF_BLOCK_SIZE - 2)
		len = TF_BLOCK_SIZE - 2;

	if (len >= buf_size)
		len = buf_size - 1;

	memcpy(buf, &sector_buf[2], len);
	buf[len] = '\0';

	return 1;
}

uint8_t TF_Card_AppendString(uint32_t start_sector, const char *str)
{
	uint8_t buf[TF_BLOCK_SIZE];
	uint32_t sector;
	uint32_t max_sector;
	uint16_t index;

	if (str == NULL)
		return 0;

	if (!TF_Card_IsReady())
		return 0;

	max_sector = TF_Card_GetBlockCount();
	if (max_sector == 0)
		return 0;

	for (sector = start_sector; sector < max_sector; sector++)
	{
		memset(buf, 0, TF_BLOCK_SIZE);
		if (TF_Card_ReadBlock(sector, buf) == 0)
			continue;

		index = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);

		if (index == 0xFFFF)
			index = 0;

		if (index == 0)
		{
			if (TF_Card_WriteString(sector, str) == 0)
				return 0;

			return 1;
		}
	}

	return 0;
}

void TF_Card_EraseSectorRange(uint32_t start_sector, uint32_t end_sector)
{
	uint32_t sector;
	uint8_t buf[TF_BLOCK_SIZE];

	memset(buf, 0xFF, TF_BLOCK_SIZE);

	for (sector = start_sector; sector <= end_sector; sector++)
	{
		TF_Card_WriteBlock(sector, buf);
	}

	printf("TF Card erased sectors %lu ~ %lu\r\n", start_sector, end_sector);
}

/* FATFS file-level API */

uint8_t TF_Card_MountFS(void)
{
	FRESULT res;

	if (fs_mounted)
		return 1;

	res = f_mount(&tf_fatfs, "0:", 1);
	if (res == FR_OK)
	{
		fs_mounted = 1;
		printf("TF Card FS mounted OK\r\n");
		return 1;
	}
	else if (res == FR_NO_FILESYSTEM)
	{
		printf("TF Card no filesystem, try format...\r\n");
		if (TF_Card_Format())
		{
			res = f_mount(&tf_fatfs, "0:", 1);
			if (res == FR_OK)
			{
				fs_mounted = 1;
				printf("TF Card FS mounted after format OK\r\n");
				return 1;
			}
		}
	}

	printf("TF Card FS mount failed, err=%d\r\n", res);
	return 0;
}

uint8_t TF_Card_UnmountFS(void)
{
	if (fs_mounted)
	{
		f_mount(NULL, "0:", 1);
		fs_mounted = 0;
	}
	return 1;
}

uint8_t TF_Card_Format(void)
{
	FRESULT res;
	BYTE work[FF_MAX_SS];
	MKFS_PARM opt;

	if (!TF_Card_IsReady())
		return 0;

	opt.fmt = FM_FAT32;
	opt.n_fat = 1;
	opt.n_root = 0;
	opt.align = 0;
	opt.au_size = 0;

	res = f_mkfs("0:", &opt, work, sizeof(work));
	if (res != FR_OK)
	{
		printf("TF Card format failed, err=%d\r\n", res);
		return 0;
	}

	printf("TF Card format OK (FAT32)\r\n");
	return 1;
}

uint8_t TF_Card_CreateFile(const char *filename)
{
	FIL fil;
	FRESULT res;

	if (!fs_mounted)
		return 0;

	res = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK)
	{
		printf("TF CreateFile failed, err=%d\r\n", res);
		return 0;
	}

	f_close(&fil);
	return 1;
}

uint8_t TF_Card_WriteFile(const char *filename, const char *data)
{
	FIL fil;
	FRESULT res;
	UINT bw;

	if (!fs_mounted || data == NULL)
		return 0;

	res = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK)
	{
		printf("TF WriteFile open failed, err=%d\r\n", res);
		return 0;
	}

	res = f_write(&fil, data, (UINT)strlen(data), &bw);
	if (res != FR_OK)
	{
		printf("TF WriteFile write failed, err=%d\r\n", res);
		f_close(&fil);
		return 0;
	}

	f_close(&fil);
	printf("TF WriteFile OK, %u bytes -> %s\r\n", bw, filename);
	return 1;
}

uint8_t TF_Card_AppendFile(const char *filename, const char *data)
{
	if (data == NULL)
		return 0;

	return TF_Card_AppendFileLen(filename, data, (uint32_t)strlen(data));
}

uint8_t TF_Card_AppendFileLen(const char *filename, const char *data, uint32_t len)
{
	FIL fil;
	FRESULT res;
	UINT bw;

	if (!fs_mounted || data == NULL)
		return 0;

	res = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
	if (res != FR_OK)
	{
		res = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);
		if (res != FR_OK)
		{
			printf("TF AppendFile open failed, err=%d\r\n", res);
			return 0;
		}
	}

	res = f_write(&fil, data, (UINT)len, &bw);
	if (res != FR_OK)
	{
		printf("TF AppendFile write failed, err=%d\r\n", res);
		f_close(&fil);
		return 0;
	}

	if (bw != (UINT)len)
	{
		printf("TF AppendFile short write, req=%lu, bw=%u\r\n", len, bw);
		f_close(&fil);
		return 0;
	}

	f_close(&fil);
	return 1;
}

uint8_t TF_Card_ReadFile(const char *filename, char *buf, uint16_t buf_size)
{
	FIL fil;
	FRESULT res;
	UINT br;

	if (!fs_mounted || buf == NULL || buf_size == 0)
		return 0;

	res = f_open(&fil, filename, FA_READ);
	if (res != FR_OK)
	{
		printf("TF ReadFile open failed, err=%d\r\n", res);
		return 0;
	}

	res = f_read(&fil, buf, buf_size - 1, &br);
	buf[br] = '\0';
	f_close(&fil);

	if (res != FR_OK)
	{
		printf("TF ReadFile read failed, err=%d\r\n", res);
		return 0;
	}

	return 1;
}

uint32_t TF_Card_GetFileSize(const char *filename)
{
	FILINFO fno;

	if (!fs_mounted)
		return 0;

	if (f_stat(filename, &fno) != FR_OK)
		return 0;

	return fno.fsize;
}

uint8_t TF_Card_DeleteFile(const char *filename)
{
	FRESULT res;

	if (!fs_mounted)
		return 0;

	res = f_unlink(filename);
	if (res != FR_OK)
	{
		printf("TF DeleteFile failed, err=%d\r\n", res);
		return 0;
	}

	printf("TF DeleteFile OK: %s\r\n", filename);
	return 1;
}

uint8_t TF_Card_FileExists(const char *filename)
{
	FILINFO fno;

	if (!fs_mounted)
		return 0;

	if (f_stat(filename, &fno) != FR_OK)
		return 0;

	return 1;
}

uint32_t TF_Card_GetFreeMB(void)
{
	FATFS *fs;
	DWORD free_clst;
	uint32_t free_mb;

	if (!fs_mounted)
		return 0;

	if (f_getfree("0:", &free_clst, &fs) != FR_OK)
		return 0;

	free_mb = ((uint64_t)free_clst * fs->csize * 512) >> 20;
	return free_mb;
}

void TF_Card_ListFiles(void)
{
	DIR dir;
	FILINFO fno;
	uint32_t count = 0;

	if (!fs_mounted)
		return;

	if (f_opendir(&dir, "0:") != FR_OK)
	{
		printf("TF Card open dir failed\r\n");
		return;
	}

	printf("===== TF Card Files =====\r\n");

	for (;;)
	{
		if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
			break;

		if (fno.fattrib & AM_DIR)
		{
			printf("[DIR]  %s\r\n", fno.fname);
		}
		else
		{
			printf("%8lu  %s\r\n", fno.fsize, fno.fname);
		}
		count++;
	}

	printf("===== %lu files =====\r\n", count);
	f_closedir(&dir);
}

