/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for FatFs     (C)ChaN, 2017                 */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "sd_diskio.h"

#define SD_CARD     0

DSTATUS disk_status (
	BYTE pdrv
)
{
	if (pdrv == SD_CARD)
	{
		if (get_sd_card_state() == 0)
			return 0;
		else
			return STA_NOINIT;
	}
	return STA_NOINIT;
}

#if 0
DSTATUS disk_initialize (
	BYTE pdrv
)
{
	uint8_t res = 0;

	switch (pdrv)
	{
		case SD_CARD:
			res = sd_init(0);
			break;
		default:
			res = 1;
			break;
	}

	if (res)
		return STA_NOINIT;
	else
		return 0;
}
#endif

DSTATUS disk_initialize (
	BYTE pdrv
)
{
	if (pdrv != SD_CARD)
	{
		return STA_NOINIT;
	}

	/* If TF_Card_Init has already brought the card up successfully,
	   avoid reinitializing SDMMC again during f_mount(). */
	if ((g_sd_ready != 0U) && (get_sd_card_state() == 0U))
	{
		return 0;
	}

	return (sd_init(0) == 0U) ? 0 : STA_NOINIT;
}

DRESULT disk_read (
	BYTE pdrv,
	BYTE *buff,
	DWORD sector,
	UINT count
)
{
	uint8_t res = 0;

	if (!count)
		return RES_PARERR;

	switch (pdrv)
	{
		case SD_CARD:
			res = sd_read_disk(buff, sector, count);
			break;
		default:
			res = 1;
			break;
	}

	if (res == 0x00)
		return RES_OK;
	else
		return RES_ERROR;
}

DRESULT disk_write (
	BYTE pdrv,
	const BYTE *buff,
	DWORD sector,
	UINT count
)
{
	uint8_t res = 0;

	if (!count)
		return RES_PARERR;

	switch (pdrv)
	{
		case SD_CARD:
			if (((uint32_t)buff) % 4 != 0)
			{
				unsigned int CountNum = 0;
				while (CountNum < count)
				{
					static unsigned char DMA_Buffer[512] __attribute__((aligned (4)));
					memcpy(&DMA_Buffer, buff + 512 * CountNum, 512);
					res = sd_write_disk(&DMA_Buffer[0], sector + CountNum, 1);
					if (res != 0x00) return RES_ERROR;
					CountNum++;
				}
			}
			else
			{
				res = sd_write_disk((unsigned char *)buff, sector, count);
			}
			break;
		default:
			res = 1;
			break;
	}

	if (res == 0x00)
		return RES_OK;
	else
		return RES_ERROR;
}

DRESULT disk_ioctl (
	BYTE pdrv,
	BYTE cmd,
	void *buff
)
{
	DRESULT res;

	if (pdrv == SD_CARD)
	{
		switch (cmd)
		{
			case CTRL_SYNC:
				res = RES_OK;
				break;
			case GET_SECTOR_SIZE:
				*(DWORD *)buff = 512;
				res = RES_OK;
				break;
			case GET_BLOCK_SIZE:
				*(DWORD *)buff = g_sd_card_info_handle.LogBlockSize;
				res = RES_OK;
				break;
			case GET_SECTOR_COUNT:
				*(DWORD *)buff = g_sd_card_info_handle.LogBlockNbr;
				res = RES_OK;
				break;
			default:
				res = RES_PARERR;
				break;
		}
	}
	else
	{
		res = RES_ERROR;
	}

	return res;
}

DWORD get_fattime(void)
{
	return 0;
}

