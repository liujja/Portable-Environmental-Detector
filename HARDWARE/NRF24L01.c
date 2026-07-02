#include "NRF24L01.h"
#include "usart.h"
#include "shared_bus_lock.h"

#define TX_PLOAD_WIDTH  NRF24_MAX_PAYLOAD_SIZE
#define RX_PLOAD_WIDTH  NRF24_MAX_PAYLOAD_SIZE

volatile NRF24_GPS_Rx_t g_nrf24_rx_gps = {0};

void NRF_Delay(void)//168MHz F:4.09MHz
{
	for(int i=0;i < 1000;i++)
	{
		__nop();
	}
}




/* ======================== SPI======================== */
static uint8_t SPI_RW(uint8_t byte)
{
    uint8_t i, rx = 0;
    for (i = 0; i < 8; i++) 
	 {
        if (byte & 0x80) NRF24L01_MOSI_HIGH(); else NRF24L01_MOSI_LOW();
        byte <<= 1;
        NRF_Delay();
        NRF24L01_SCK_HIGH();
        NRF_Delay();
		    rx <<= 1;
        if (NRF24L01_MISO_READ()) rx |= 0x01;
        NRF24L01_SCK_LOW();
        NRF_Delay();
    }
    return rx;
}


static uint8_t NRF24_ReadReg(uint8_t reg) {
    uint8_t val;
    NRF24L01_CSN_LOW(); NRF_Delay();
    SPI_RW(NRF24_CMD_R_REGISTER | (reg & 0x1F));
    val = SPI_RW(0xFF);
    NRF24L01_CSN_HIGH(); NRF_Delay();
    return val;
}


/* SPI */
static void NRF24_WriteReg(uint8_t reg, uint8_t val) 
{
    NRF24L01_CSN_LOW(); 
	  NRF_Delay();
    SPI_RW(NRF24_CMD_W_REGISTER | (reg & 0x1F));
    SPI_RW(val);
    NRF24L01_CSN_HIGH(); 
	  NRF_Delay();
}

static void NRF24_WriteBuf(uint8_t reg, uint8_t *buf, uint8_t len) 
{
	
	  uint8_t i;

    if (buf == NULL || len == 0)
    {
        return;
    }
		
    NRF24L01_CSN_LOW(); 
	  NRF_Delay();
		
    SPI_RW(NRF24_CMD_W_REGISTER | (reg & 0x1F));
        for (i = 0; i < len; i++)
    {
        SPI_RW(buf[i]);
    }
		
    NRF24L01_CSN_HIGH(); 
	  NRF_Delay();
}

static void NRF24_FlushTx(void)
{
    NRF24L01_CSN_LOW();
    NRF_Delay();

    SPI_RW(NRF24_CMD_FLUSH_TX);

    NRF24L01_CSN_HIGH();
    NRF_Delay();
}

static void NRF24_FlushRx(void)
{
    NRF24L01_CSN_LOW();
    NRF_Delay();

    SPI_RW(NRF24_CMD_FLUSH_RX);

    NRF24L01_CSN_HIGH();
    NRF_Delay();
}
static void NRF24_WriteTxPayload(const uint8_t *buf, uint8_t len)
{
    uint8_t i;

    if (buf == NULL || len == 0)
    {
        return;
    }

    NRF24L01_CSN_LOW();
    NRF_Delay();

    SPI_RW(NRF24_CMD_W_TX_PAYLOAD);

    for (i = 0; i < len; i++)
    {
        SPI_RW(buf[i]);
    }

    NRF24L01_CSN_HIGH();
    NRF_Delay();
}
static void NRF24_ReadRxPayload(uint8_t *buf, uint8_t len)
{
    uint8_t i;

    if (buf == NULL || len == 0)
    {
        return;
    }

    NRF24L01_CSN_LOW();
    NRF_Delay();

    SPI_RW(NRF24_CMD_R_RX_PAYLOAD);

    for (i = 0; i < len; i++)
    {
        buf[i] = SPI_RW(NRF24_CMD_NOP);
    }

    NRF24L01_CSN_HIGH();
    NRF_Delay();
}

/* ======================== GPIO======================== */
void NRF24L01_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
  
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    
   
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
   
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
        
   
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
   
    // CE - PD3 
    GPIO_InitStruct.Pin = NRF24L01_CE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(NRF24L01_CE_PORT, &GPIO_InitStruct);
    
    // IRQ - PD4 
    GPIO_InitStruct.Pin = NRF24L01_IRQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(NRF24L01_IRQ_PORT, &GPIO_InitStruct);
    
    // CSN - PI7 
    GPIO_InitStruct.Pin = NRF24L01_CSN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(NRF24L01_CSN_PORT, &GPIO_InitStruct);
    
    
    NRF24L01_CE_LOW();
		NRF_Delay();
    NRF24L01_CSN_HIGH();
		NRF_Delay();
		NRF24L01_SCK_LOW();
		NRF_Delay();
    NRF24L01_MOSI_LOW();
		NRF_Delay();
}


 
void NRF24L01_Init(uint8_t mode)
{
    if (!SharedBus_Lock(50))
        return;

    NRF24L01_GPIO_Init();
    SharedBus_PrepareForNRF24();
    NRF_Delay();

    NRF24L01_CE_LOW();
    NRF_Delay();

    NRF24_WriteReg(NRF24_REG_EN_AA, 0x00);
    NRF24_WriteReg(NRF24_REG_EN_RXADDR, 0x01);
    NRF24_WriteReg(NRF24_REG_SETUP_AW, 0x03);
    NRF24_WriteReg(NRF24_REG_SETUP_RETR, 0x00);
    NRF24_WriteReg(NRF24_REG_RF_CH, 40);
    NRF24_WriteReg(NRF24_REG_RX_PW_P0, NRF24_PLOAD_WIDTH);

    if (mode == 1) {
        NRF24_WriteReg(NRF24_REG_RF_SETUP, 0x06);
    } else {
        NRF24_WriteReg(NRF24_REG_RF_SETUP, 0x07);
    }

    NRF24_WriteBuf(NRF24_REG_RX_ADDR_P0, NRF24_ADDRESS, 5);
    NRF24_WriteBuf(NRF24_REG_TX_ADDR,    NRF24_ADDRESS, 5);

    if (mode == 1) {
        NRF24_WriteReg(NRF24_REG_CONFIG, 0x0F);
        NRF24L01_CE_HIGH();
    } else {
        NRF24_WriteReg(NRF24_REG_CONFIG, 0x0E);
        NRF24L01_CE_LOW();
    }

    SharedBus_PrepareForNRF24();
    SharedBus_Unlock();
}


void NRF24L01_SetMode(uint8_t mode)
{
    if (!SharedBus_Lock(20))
        return;

    SharedBus_PrepareForNRF24();

    NRF24L01_CE_LOW();
    NRF_Delay();

    /* 清中断标志 */
    NRF24_WriteReg(NRF24_REG_STATUS, 0x70);

    if (mode == 1)
    {
        /* RX mode */
        NRF24_FlushRx();
        NRF24_WriteReg(NRF24_REG_CONFIG, 0x0F);  /* PWR_UP=1, PRIM_RX=1, CRC=2 bytes */
        NRF24L01_CE_HIGH();
        NRF_Delay();
    }
    else
    {
        /* TX mode */
        NRF24_FlushTx();
        NRF24_WriteReg(NRF24_REG_CONFIG, 0x0E);  /* PWR_UP=1, PRIM_RX=0, CRC=2 bytes */
        NRF_Delay();
        NRF24L01_CE_LOW();
    }

    SharedBus_PrepareForNRF24();
    SharedBus_Unlock();
}

//static uint8_t NRF24_ReadStatus(void)
//{
//    uint8_t status;

//    NRF24L01_CSN_LOW();
//    NRF_Delay();

//    status = SPI_RW(NRF24_CMD_NOP);   

//    NRF24L01_CSN_HIGH();
//    NRF_Delay();

//    return status;
//}


void NRF24L01_SendPacket(uint8_t *data, uint8_t len) {
    uint8_t buf[NRF24_PLOAD_WIDTH] = {0};
    if (len > NRF24_PLOAD_WIDTH) len = NRF24_PLOAD_WIDTH;
    memcpy(buf, data, len);

 
    NRF24L01_CE_LOW();
		NRF_Delay();
    NRF24_WriteReg(NRF24_REG_CONFIG, 0x0A); // PRIM_RX=0

    
    NRF24_WriteReg(NRF24_CMD_FLUSH_TX, 0xFF);
    NRF24_WriteReg(NRF24_REG_STATUS, 0x70);

    NRF24_WriteBuf(NRF24_CMD_W_TX_PAYLOAD, buf, NRF24_PLOAD_WIDTH);
    NRF24L01_CE_HIGH();
    NRF_Delay();

    
    uint16_t timeout = 200;
    uint8_t status;
    while(timeout--) {
        status = NRF24_ReadReg(NRF24_REG_STATUS);
        if (status & 0x20) break;
			  if (status & 0x10) break;
        NRF_Delay();
    }

  
    NRF24L01_CE_LOW();
    NRF_Delay();
}


uint8_t NRF24L01_ReceivePacket(uint8_t *data)
{
    uint8_t status;
    uint8_t fifo_before;
    uint8_t fifo_after;

    if (data == NULL)
        return 0;

    if (!SharedBus_Lock(20))
        return 0;

    SharedBus_PrepareForNRF24();

    status = NRF24_ReadReg(NRF24_REG_STATUS);
    if (status & 0x40)
    {
        fifo_before = NRF24_ReadReg(NRF24_REG_FIFO_STATUS);
        NRF24_ReadRxPayload(data, NRF24_PLOAD_WIDTH);
        fifo_after = NRF24_ReadReg(NRF24_REG_FIFO_STATUS);

        NRF24_WriteReg(NRF24_REG_STATUS, 0x40);

        (void)fifo_before;
        (void)fifo_after;

        SharedBus_PrepareForNRF24();
        SharedBus_Unlock();
        return 1;
    }

    SharedBus_PrepareForNRF24();
    SharedBus_Unlock();
    return 0;
}


static uint8_t NRF24_CalcChecksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum = 0;

    if (buf == NULL)
        return 0;

    for (uint8_t i = 0; i < len; i++)
    {
        sum += buf[i];
    }

    return sum;
}

static void NRF24_GetBeijingTime(const GPS_Position_t *pos,
                                 uint8_t *hour,
                                 uint8_t *minute,
                                 uint8_t *second)
{
    uint16_t total_seconds;
    uint8_t  h, m, s;

    if ((pos == NULL) || (hour == NULL) || (minute == NULL) || (second == NULL))
        return;

    h = pos->hour;
    m = pos->minute;
    s = pos->second;

    total_seconds = (uint16_t)h * 3600U + (uint16_t)m * 60U + (uint16_t)s;
    total_seconds = (uint16_t)((total_seconds + 8U * 3600U) % 86400U);

    *hour   = (uint8_t)(total_seconds / 3600U);
    *minute = (uint8_t)((total_seconds % 3600U) / 60U);
    *second = (uint8_t)(total_seconds % 60U);
}


void NRF24_ReceiveGpsLocation(void)
{
    uint8_t rxBuf[NRF24_PLOAD_WIDTH];
    uint8_t sum;
    uint8_t status;

    if (!SharedBus_Lock(20))
        return;

    SharedBus_PrepareForNRF24();

    status = NRF24_ReadReg(NRF24_REG_STATUS);
    if ((status & 0x40) == 0)
    {
        SharedBus_PrepareForNRF24();
        SharedBus_Unlock();
        return;
    }

    memset(rxBuf, 0, sizeof(rxBuf));

    NRF24_ReadRxPayload(rxBuf, NRF24_PLOAD_WIDTH);
    NRF24_WriteReg(NRF24_REG_STATUS, 0x40);

    if ((rxBuf[0] == NRF24_GPS_HEAD0) && (rxBuf[1] == NRF24_GPS_HEAD1))
    {
        sum = NRF24_CalcChecksum(rxBuf, NRF24_PLOAD_WIDTH - 1);
        if (sum == rxBuf[NRF24_PLOAD_WIDTH - 1])
        {
            memcpy((void *)&g_nrf24_rx_gps.lat, &rxBuf[2], sizeof(g_nrf24_rx_gps.lat));
            memcpy((void *)&g_nrf24_rx_gps.lon, &rxBuf[6], sizeof(g_nrf24_rx_gps.lon));
            g_nrf24_rx_gps.hour       = rxBuf[10];
            g_nrf24_rx_gps.minute     = rxBuf[11];
            g_nrf24_rx_gps.second     = rxBuf[12];
            g_nrf24_rx_gps.data_valid = rxBuf[13];
            g_nrf24_rx_gps.frame_valid = 1;
            g_nrf24_rx_gps.tick_ms    = HAL_GetTick();

            printf("RX GPS(BJT): lat=%.7f lon=%.7f time=%02u:%02u:%02u gps_valid=%u seq=0x%02X\r\n",
                   g_nrf24_rx_gps.lat / 10000000.0,
                   g_nrf24_rx_gps.lon / 10000000.0,
                   g_nrf24_rx_gps.hour,
                   g_nrf24_rx_gps.minute,
                   g_nrf24_rx_gps.second,
                   g_nrf24_rx_gps.data_valid,
                   rxBuf[14]);
        }
        else
        {
            printf("RX GPS checksum error: calc=0x%02X recv=0x%02X\r\n",
                   sum, rxBuf[NRF24_PLOAD_WIDTH - 1]);
        }
    }
    else
    {
        printf("RX GPS frame header error: %02X %02X\r\n", rxBuf[0], rxBuf[1]);
    }

    SharedBus_PrepareForNRF24();
    SharedBus_Unlock();
}


uint8_t NRF24_SendGpsLocation(const GPS_Position_t *pos)
{
    static uint8_t seq = 0;
    uint8_t txBuf[NRF24_PLOAD_WIDTH] = {0};
    GPS_Position_t empty_pos = {0};
    uint8_t bj_hour = 0;
    uint8_t bj_minute = 0;
    uint8_t bj_second = 0;
    uint8_t valid = 0;
    uint8_t status = 0;
    uint16_t timeout = 2000;
    uint8_t ok = 0;

    if (pos == NULL)
    {
        pos = &empty_pos;
    }

    valid = (uint8_t)(pos->data_valid && pos->time_valid);

    if (valid)
    {
        NRF24_GetBeijingTime(pos, &bj_hour, &bj_minute, &bj_second);
    }

    txBuf[0] = NRF24_GPS_HEAD0;
    txBuf[1] = NRF24_GPS_HEAD1;

    if (valid)
    {
        memcpy(&txBuf[2], &pos->lat, sizeof(pos->lat));
        memcpy(&txBuf[6], &pos->lon, sizeof(pos->lon));
    }
    else
    {
        int32_t zero = 0;
        memcpy(&txBuf[2], &zero, sizeof(zero));
        memcpy(&txBuf[6], &zero, sizeof(zero));
    }

    txBuf[10] = bj_hour;
    txBuf[11] = bj_minute;
    txBuf[12] = bj_second;
    txBuf[13] = valid;
    txBuf[14] = seq++;
    txBuf[15] = NRF24_CalcChecksum(txBuf, NRF24_PLOAD_WIDTH - 1);

    {
        double lat_print = 0.0;
        double lon_print = 0.0;

        if (valid)
        {
            lat_print = pos->lat / 10000000.0;
            lon_print = pos->lon / 10000000.0;
        }

        printf("TX GPS(BJT): lat=%.7f lon=%.7f time=%02u:%02u:%02u valid=%u seq=0x%02X\r\n",
               lat_print, lon_print,
               txBuf[10], txBuf[11], txBuf[12],
               txBuf[13], txBuf[14]);
    }

    NRF24L01_SetMode(0);  /* TX mode */

    if (!SharedBus_Lock(20))
    {
        NRF24L01_SetMode(1);
        return 0;
    }

    SharedBus_PrepareForNRF24();

    NRF24L01_CE_LOW();
    NRF_Delay();

    NRF24_WriteReg(NRF24_REG_STATUS, 0x70);
    NRF24_FlushTx();
    NRF24_WriteTxPayload(txBuf, NRF24_PLOAD_WIDTH);

    /* CE 拉高开始发送 */
    NRF24L01_CE_HIGH();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF_Delay();
    NRF24L01_CE_LOW();

    /* 等待发送完成或失败 */
    while (timeout--)
    {
        status = NRF24_ReadReg(NRF24_REG_STATUS);

        if (status & 0x20)   /* TX_DS */
        {
            ok = 1;
            break;
        }

        if (status & 0x10)   /* MAX_RT */
        {
            ok = 0;
            break;
        }

        NRF_Delay();
    }

    if (ok)
    {
        printf("NRF TX OK, STATUS=0x%02X\r\n", status);
    }
    else if (status & 0x10)
    {
        printf("NRF TX MAX_RT, STATUS=0x%02X\r\n", status);
    }
    else
    {
        printf("NRF TX TIMEOUT, STATUS=0x%02X\r\n", status);
    }

    NRF24_WriteReg(NRF24_REG_STATUS, 0x70);
    NRF24_FlushTx();

    SharedBus_PrepareForNRF24();
    SharedBus_Unlock();

    NRF24L01_SetMode(1);  /* back to RX */

    return ok;
}



