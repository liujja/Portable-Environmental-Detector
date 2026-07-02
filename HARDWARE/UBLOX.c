#include "UBLOX.h"
#include <stdio.h>
#include <string.h> 
#include <math.h>
#include <stdlib.h>
#include "usart.h"
#include "delay.h"

/* Global variables */
GPS_Position_t gps_position;
volatile uint32_t gps_rx_byte_count = 0U;
volatile uint32_t gps_nmea_sentence_count = 0U;
volatile uint32_t gps_ubx_pvt_count = 0U;
volatile uint32_t gps_last_rx_tick_ms = 0U;


/* UBX protocol constants */
#define UBX_SYNC1       0xB5
#define UBX_SYNC2       0x62
#define UBX_CLASS_NAV   0x01
#define UBX_ID_PVT      0x07
#define PVT_MSG_LEN     100          /* PVT message: 92 bytes payload + 8 bytes header/footer */
#define UBX_MAX_LEN     128

#define UBX_CLASS_CFG   0x06
#define UBX_ID_CFG_MSG  0x01
#define UBX_ID_CFG_RATE 0x08

/* Parse state */
static uint8_t  pvt_buf[UBX_MAX_LEN]; /* PVT message buffer */
static uint16_t pvt_index = 0;        /* current index */
static uint16_t pvt_length = 0;     
static uint16_t pvt_frame_len = 0;  
static uint8_t  pvt_sync   = 0;       /* sync state: 0=wait SYNC1, 1=wait SYNC2, 2=receiving */
static char     nmea_buf[128];
static uint16_t nmea_index = 0;
static uint8_t  nmea_active = 0;

static void GPS_Parse_NMEA(void);
static void GPS_Process_NMEA_Byte(uint8_t data);
static int32_t GPS_NmeaDegTo1e7(const char *field, char hemi, uint8_t is_lat);
static uint8_t GPS_ParseNmeaTime(const char *field, uint8_t *hour, uint8_t *minute, uint8_t *second);

/**
 * @brief   Send UBX config message to GPS module via USART1
 * @param   msg_class: UBX class byte
 * @param   msg_id: UBX ID byte
 * @param   len: payload length
 * @param   payload: payload data
 * @retval  None
 */
static void GPS_Send_UBX(uint8_t msg_class, uint8_t msg_id, uint16_t len, const uint8_t *payload)
{
    uint8_t ck_a = 0, ck_b = 0;
    uint16_t i;

    /* Build and send UBX frame */
    uint8_t buf[2];
    buf[0] = UBX_SYNC1; buf[1] = UBX_SYNC2;
    HAL_UART_Transmit(&huart1, buf, 2, 100);

    buf[0] = msg_class; buf[1] = msg_id;
    ck_a += buf[0]; ck_b += ck_a;
    ck_a += buf[1]; ck_b += ck_a;
    HAL_UART_Transmit(&huart1, buf, 2, 100);

    buf[0] = (uint8_t)(len & 0xFF); buf[1] = (uint8_t)((len >> 8) & 0xFF);
    ck_a += buf[0]; ck_b += ck_a;
    ck_a += buf[1]; ck_b += ck_a;
    HAL_UART_Transmit(&huart1, buf, 2, 100);

    for (i = 0; i < len; i++)
    {
        ck_a += payload[i];
        ck_b += ck_a;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)payload, len, 100);

    buf[0] = ck_a; buf[1] = ck_b;
    HAL_UART_Transmit(&huart1, buf, 2, 100);
}

/**
 * @brief   Initialize GPS module: configure to output UBX-NAV-PVT messages
 * @param   None
 * @retval  None
 * @note    Call after usart1_init(). Configures GPS to output NAV-PVT at 1Hz via UART1.
 *          Sends UBX-CFG-MSG (enable NAV-PVT on UART1) and UBX-CFG-RATE (200ms).
 */
void GPS_Init(void)
{
    /* Wait for GPS module to boot */
    delayms(1000);

    /* UBX-CFG-MSG: Enable NAV-PVT (0x01 0x07) on UART1 */
				uint8_t cfg_msg[] = {
						0x01, 0x07,    /* msgClass=NAV, msgID=PVT */
						0,             /* DDC/I2C */
						1,             /* UART1 */
						0,             /* UART2 */
						0,             /* USB */
						0,             /* SPI */
						0              /* Reserved */
				};
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_msg), cfg_msg);
        delayms(100);
    

    /* UBX-CFG-RATE: Set measurement rate to 200ms (5Hz measurement, 1Hz nav output) */
    {
        uint8_t cfg_rate[] = {
            0xC8, 0x00,    /* measRate = 200ms */
            0x01, 0x00,    /* navRate = 1 (output every measurement) */
            0x01, 0x00     /* timeRef = GPS time */
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_RATE, sizeof(cfg_rate), cfg_rate);
        delayms(100);
    }

    /* Keep NMEA GGA enabled as a fallback when UBX NAV-PVT is absent. */
    {
        uint8_t cfg_nmea[] = {
            0xF0, 0x00,    /* msgClass=NMEA Standard, msgID=GGA */
            0,              /* DDC/I2C */
            1,              /* UART1: enabled */
            0,              /* UART2 */
            0,              /* USB */
            0,              /* SPI */
            0               /* Reserved */
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_nmea), cfg_nmea);
        delayms(100);
    }
    /* Disable NMEA GLL */
    {
        uint8_t cfg_nmea[] = {
            0xF0, 0x01,    /* NMEA GLL */
            0, 0, 0, 0, 0, 0
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_nmea), cfg_nmea);
        delayms(100);
    }
    /* Disable NMEA GSA */
    {
        uint8_t cfg_nmea[] = {
            0xF0, 0x02,    /* NMEA GSA */
            0, 0, 0, 0, 0, 0
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_nmea), cfg_nmea);
        delayms(100);
    }
    /* Disable NMEA GSV */
    {
        uint8_t cfg_nmea[] = {
            0xF0, 0x03,    /* NMEA GSV */
            0, 0, 0, 0, 0, 0
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_nmea), cfg_nmea);
        delayms(100);
    }
    /* Disable NMEA RMC */
    {
        uint8_t cfg_nmea[] = {
            0xF0, 0x04,    /* NMEA RMC */
            0, 0, 0, 0, 0, 0
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_nmea), cfg_nmea);
        delayms(100);
    }
    /* Disable NMEA VTG */
    {
        uint8_t cfg_nmea[] = {
            0xF0, 0x05,    /* NMEA VTG */
            0, 0, 0, 0, 0, 0
        };
        GPS_Send_UBX(UBX_CLASS_CFG, UBX_ID_CFG_MSG, sizeof(cfg_nmea), cfg_nmea);
        delayms(100);
    }

    printf("GPS Init done\r\n");
}

static int32_t GPS_NmeaDegTo1e7(const char *field, char hemi, uint8_t is_lat)
{
    double raw;
    double deg;
    double minutes;
    int deg_digits;
    double decimal_deg;

    if ((field == NULL) || (field[0] == '\0'))
    {
        return 0;
    }

    raw = atof(field);
    deg_digits = is_lat ? 2 : 3;
    deg = floor(raw / pow(10.0, 2.0));

    if (deg_digits == 3)
    {
        deg = floor(raw / 100.0);
    }

    minutes = raw - (deg * 100.0);
    decimal_deg = deg + (minutes / 60.0);

    if ((hemi == 'S') || (hemi == 'W'))
    {
        decimal_deg = -decimal_deg;
    }

    return (int32_t)(decimal_deg * 10000000.0);
}

static uint8_t GPS_ParseNmeaTime(const char *field, uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    int hh, mm, ss;

    if ((field == NULL) || (field[0] == '\0') ||
        (hour == NULL) || (minute == NULL) || (second == NULL))
    {
        return 0;
    }

    if (sscanf(field, "%2d%2d%2d", &hh, &mm, &ss) != 3)
    {
        return 0;
    }

    if ((hh < 0) || (hh > 23) || (mm < 0) || (mm > 59) || (ss < 0) || (ss > 59))
    {
        return 0;
    }

    *hour = (uint8_t)hh;
    *minute = (uint8_t)mm;
    *second = (uint8_t)ss;
    return 1;
}

static void GPS_Parse_NMEA(void)
{
    char line[128];
    char *fields[20];
    char *token;
    char *next;
    int field_count = 0;
    uint8_t hh, mm, ss;
    int32_t lat, lon;
    int fix_quality;
    int sats;

    strncpy(line, nmea_buf, sizeof(line) - 1U);
    line[sizeof(line) - 1U] = '\0';

    token = line;
    while ((token != NULL) && (field_count < (int)(sizeof(fields) / sizeof(fields[0]))))
    {
        fields[field_count++] = token;
        next = strchr(token, ',');
        if (next != NULL)
        {
            *next = '\0';
            token = next + 1;
        }
        else
        {
            next = strpbrk(token, "\r\n");
            if (next != NULL)
            {
                *next = '\0';
            }
            token = NULL;
        }
    }

    if (field_count < 7)
    {
        return;
    }

    if ((strcmp(fields[0], "$GPGGA") != 0) &&
        (strcmp(fields[0], "$GNGGA") != 0) &&
        (strcmp(fields[0], "$BDGGA") != 0))
    {
        return;
    }

    if (GPS_ParseNmeaTime(fields[1], &hh, &mm, &ss))
    {
        gps_position.hour = hh;
        gps_position.minute = mm;
        gps_position.second = ss;
        gps_position.time_valid = 1;
    }

    fix_quality = atoi(fields[6]);
    sats = (field_count > 7) ? atoi(fields[7]) : 0;
    gps_position.satellites = (uint8_t)((sats < 0) ? 0 : sats);

    if (fix_quality <= 0)
    {
        gps_position.fix_type = 0;
        gps_position.data_valid = 0;
        return;
    }

    lat = GPS_NmeaDegTo1e7(fields[2], (field_count > 3 && fields[3][0] != '\0') ? fields[3][0] : 'N', 1);
    lon = GPS_NmeaDegTo1e7(fields[4], (field_count > 5 && fields[5][0] != '\0') ? fields[5][0] : 'E', 0);

    gps_position.lat = lat;
    gps_position.lon = lon;
    gps_position.data_valid = 1;

    switch (fix_quality)
    {
        case 1: gps_position.fix_type = 3; break; /* GPS fix */
        case 2: gps_position.fix_type = 4; break; /* DGPS */
        case 4: gps_position.fix_type = 6; break; /* RTK Fixed */
        case 5: gps_position.fix_type = 5; break; /* RTK Float */
        default: gps_position.fix_type = 2; break;
    }
}

static void GPS_Process_NMEA_Byte(uint8_t data)
{
    if (data == '$')
    {
        nmea_active = 1;
        nmea_index = 0;
        nmea_buf[nmea_index++] = (char)data;
        return;
    }

    if (!nmea_active)
    {
        return;
    }

    if (nmea_index >= (sizeof(nmea_buf) - 1U))
    {
        nmea_active = 0;
        nmea_index = 0;
        return;
    }

    nmea_buf[nmea_index++] = (char)data;

    if (data == '\n')
    {
        nmea_buf[nmea_index] = '\0';
        gps_nmea_sentence_count++;
        GPS_Parse_NMEA();
        nmea_active = 0;
        nmea_index = 0;
    }
}

static void GPS_Parse_PVT(void)
{
	
  	uint8_t valid = pvt_buf[17];
    uint8_t flags = pvt_buf[27];
    uint8_t carr_soln = (flags >> 6) & 0x03;
	
	  gps_position.hour   = pvt_buf[14];
    gps_position.minute = pvt_buf[15];
    gps_position.second = pvt_buf[16];
	
	    /* valid bit0: validDate, bit1: validTime */
    gps_position.time_valid = ((valid & 0x02) != 0U) ? 1U : 0U;


    if (!(flags & 0x01))
    {
        gps_position.fix_type = 0;
        gps_position.data_valid = 0;
        return;
    }

    gps_position.fix_type = pvt_buf[26];



    if (carr_soln == 2)
    {
        gps_position.fix_type = 6;
    }
    else if (carr_soln == 1)
    {
        gps_position.fix_type = 5;
    }
    else if (flags & 0x02)
    {
        gps_position.fix_type = 4;
    }

    gps_position.data_valid = 1;

    gps_position.satellites = pvt_buf[29];

    gps_position.lon =
        (int32_t)(
        ((uint32_t)pvt_buf[33] << 24) |
        ((uint32_t)pvt_buf[32] << 16) |
        ((uint32_t)pvt_buf[31] << 8 ) |
        ((uint32_t)pvt_buf[30]));

    gps_position.lat =
        (int32_t)(
        ((uint32_t)pvt_buf[37] << 24) |
        ((uint32_t)pvt_buf[36] << 16) |
        ((uint32_t)pvt_buf[35] << 8 ) |
        ((uint32_t)pvt_buf[34]));

    gps_position.alt =
        (int32_t)(
        ((uint32_t)pvt_buf[45] << 24) |
        ((uint32_t)pvt_buf[44] << 16) |
        ((uint32_t)pvt_buf[43] << 8 ) |
        ((uint32_t)pvt_buf[42]));
   {
    uint32_t h_acc =
        ((uint32_t)pvt_buf[49] << 24) |
        ((uint32_t)pvt_buf[48] << 16) |
        ((uint32_t)pvt_buf[47] << 8 ) |
        ((uint32_t)pvt_buf[46]);

    uint32_t v_acc =
        ((uint32_t)pvt_buf[53] << 24) |
        ((uint32_t)pvt_buf[52] << 16) |
        ((uint32_t)pvt_buf[51] << 8 ) |
        ((uint32_t)pvt_buf[50]);

    gps_position.h_acc = h_acc / 1000.0f;
    gps_position.v_acc = v_acc / 1000.0f;
  }
}

/**
 * @brief   Process received GPS byte (called in USART1 interrupt callback)
 * @param   data: received byte
 * @retval  None
 */
void GPS_Process_Byte(uint8_t data)
{
    gps_rx_byte_count++;
    gps_last_rx_tick_ms = HAL_GetTick();
    GPS_Process_NMEA_Byte(data);

    switch (pvt_sync)
    {
        case 0:     
        {
            if (data == UBX_SYNC1)
            {
                pvt_buf[0] = data;
                pvt_index = 1;
                pvt_sync = 1;
            }
        }
        break;

        case 1:    
        {
            if (data == UBX_SYNC2)
            {
                pvt_buf[1] = data;
                pvt_index = 2;
                pvt_sync = 2;
            }
            else
            {
                pvt_sync = 0;
            }
        }
        break;

        case 2:
        {
            if (pvt_index >= UBX_MAX_LEN)
            {
                pvt_sync = 0;
                pvt_index = 0;
                break;
            }

            pvt_buf[pvt_index++] = data;

 
            if (pvt_index == 4)
            {
                if (pvt_buf[2] != UBX_CLASS_NAV ||
                    pvt_buf[3] != UBX_ID_PVT)
                {
                    pvt_sync = 0;
                    pvt_index = 0;
                    break;
                }
            }

            /* payload */
            if (pvt_index == 6)
            {
                pvt_length =
                    ((uint16_t)pvt_buf[5] << 8) |
                    pvt_buf[4];

                pvt_frame_len = pvt_length + 8;

                if (pvt_frame_len > UBX_MAX_LEN)
                {
                    pvt_sync = 0;
                    pvt_index = 0;
                }
            }


            if ((pvt_frame_len > 0) &&
                (pvt_index >= pvt_frame_len))
            {
                uint8_t ck_a = 0;
                uint8_t ck_b = 0;

                for (uint16_t i = 2; i < (6 + pvt_length); i++)
                {
                    ck_a += pvt_buf[i];
                    ck_b += ck_a;
                }

                if ((ck_a == pvt_buf[6 + pvt_length]) &&
                    (ck_b == pvt_buf[7 + pvt_length]))
                {
                    gps_ubx_pvt_count++;
                    GPS_Parse_PVT();
                }

                pvt_sync = 0;
                pvt_index = 0;
                pvt_length = 0;
                pvt_frame_len = 0;
            }
        }
        break;

        default:
        {
            pvt_sync = 0;
            pvt_index = 0;
        }
        break;
    }
}

/**
 * @brief   Print GPS position via USART2 (printf)
 * @param   None
 * @retval  None
 */
void GPS_Print_Position(void)
{
    if (gps_position.data_valid && gps_position.fix_type >= 2)
    {
        double latitude  = gps_position.lat / 10000000.0;
        double longitude = gps_position.lon / 10000000.0;
        float  altitude  = gps_position.alt / 1000.0f;
        
        const char *fix_str;
        switch (gps_position.fix_type)
        {
            case 2:  fix_str = "2D";           break;
            case 3:  fix_str = "3D";           break;
            case 4:  fix_str = "DGPS";         break;
            case 5:  fix_str = "RTK-Float";    break;
            case 6:  fix_str = "RTK-Fixed";    break;
            default: fix_str = "Unknown";      break;
        }
        
        printf("\r\n========================================\r\n");
        printf("          GPS Position Info              \r\n");
        printf("========================================\r\n");
        printf(" Fix Type  : %s\r\n", fix_str);
        printf(" Satellite : %d\r\n", gps_position.satellites);
        printf(" Latitude  : %.7f\r\n", latitude);
        printf(" Longitude : %.7f\r\n", longitude);
        printf(" Altitude  : %.2f m\r\n", altitude);
        printf(" H-Accuracy: %.3f m\r\n", gps_position.h_acc);
        printf(" V-Accuracy: %.3f m\r\n", gps_position.v_acc);
        printf("========================================\r\n\r\n");
    }
    else
    {
        printf("GPS No Fix (Sats: %d)\r\n", gps_position.satellites);
    }
}

