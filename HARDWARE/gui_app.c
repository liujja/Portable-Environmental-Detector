/**
 ****************************************************************************************************
 * @file        gui_app.c
 * @brief       GUI application layer — key-driven menu + info display + thermal page
 * @note        Default: menu+info page. Thermal: only when selected from menu.
 ****************************************************************************************************
 */

#include "gui_app.h"
#include "ugui.h"
#include "ugui_port.h"
#include "key.h"
#include "bsp_lcd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "storage_manager.h"

/* ========================================================================
 * Optional sensor headers — guarded by ENABLE_BSP_* macros
 * ======================================================================== */
#if ENABLE_BSP_MLX90640
extern float mlx90640_temperatures[768];
#endif

#if ENABLE_BSP_SHT30
#include "bsp_sht30.h"
#endif

#if ENABLE_BSP_EWM201
#include "bsp_ewm201.h"
#endif

#if ENABLE_BSP_NRF24L01
#include "NRF24L01.h"
#endif

#include "SPL06.h"
extern Baro_Data Baro;

#if ENABLE_BSP_MQX
#include "bsp_MQx.h"
#endif

#if ENABLE_BSP_NEO_M8P
#include "UBLOX.h"
#endif

#if ENABLE_BSP_GEIGER
#include "bsp_geiger.h"
#endif

extern float g_battery_voltage;
extern uint8_t g_battery_percent;
extern float g_charge_voltage;
extern uint8_t g_charge_active;
extern float g_mq2_mv;
extern float g_mq4_mv;

/* ========================================================================
 * Page definitions
 * ======================================================================== */
typedef enum {
    PAGE_MAIN    = 0,  /* Menu + info (default, no thermal)  */
    PAGE_THERMAL = 1,  /* Full-screen thermal image           */
} page_t;

/* Menu items on PAGE_MAIN */
#define MENU_ITEMS         4
#define MENU_THERMAL       0
#define MENU_INTERCOM      1
#define MENU_SEND_LOC      2
#define MENU_USB_MSC       3

static const char *menu_labels[MENU_ITEMS] = {
    "Thermal Image",
    "Intercom",
    "Send Location",
    "Usb MSC",
};

/* ========================================================================
 * Application state
 * ======================================================================== */
static page_t   current_page  = PAGE_MAIN;
static uint8_t  menu_cursor   = 0;
static uint8_t  intercom_on   = 0;
static uint8_t  send_status   = 0;   /* 0=idle, 1=sending, 2=done, 3=fail */
static uint32_t send_timer    = 0;

/* ========================================================================
 * Helper: filled text bar
 * ======================================================================== */
static void DrawBar(UG_S16 x, UG_S16 y, UG_S16 w, UG_S16 h,
                    UG_COLOR bg, UG_COLOR fg, const char *text)
{
    UG_FillFrame(x, y, (UG_S16)(x + w - 1), (UG_S16)(y + h - 1), bg);
    UG_SetForecolor(fg);
    UG_SetBackcolor(bg);
    UG_PutString(x + 4, y + 2, (char*)text);
}

/* ========================================================================
 * GUI_AppInit
 * ======================================================================== */
void GUI_AppInit(void)
{
    current_page = PAGE_MAIN;
    menu_cursor  = 0;
    intercom_on  = 0;
    send_status  = 0;
}

/* ========================================================================
 * GUI_IsThermalActive — query by display_task to decide LCD_ShowThermalImage()
 * ======================================================================== */
uint8_t GUI_IsThermalActive(void)
{
    return (current_page == PAGE_THERMAL) ? 1 : 0;
}

/* ========================================================================
 * Sensor data formatting
 * ======================================================================== */

static void GetIRStr(char *buf, int len)
{
#if ENABLE_BSP_MLX90640

    float max_t = -100.0f, min_t = 100.0f;
    int i;
    for (i = 0; i < 768; i++)
    {
        float v = mlx90640_temperatures[i];
        if (v > 0.1f || v < -0.1f)
        {
            if (v < min_t) min_t = v;
            if (v > max_t) max_t = v;
        }
    }
    if (min_t <= max_t)
        snprintf(buf, len, "IR: %4.1f~%4.1f C", min_t, max_t);
    else
        snprintf(buf, len, "IR: --");
#else
    snprintf(buf, len, "IR: --");
#endif
}

static void GetSHT30Str(char *buf, int len)
{
#if ENABLE_BSP_SHT30
    snprintf(buf, len, "Env: %.1fC  %.0f%%RH", Th30BufTypedef.TH30_temp,
             Th30BufTypedef.TH30_hum);
#else
    snprintf(buf, len, "Env: --C  --%%");
#endif
}

static void GetBaroStr(char *buf, int len)
{
    if (Baro.Valid &&
        (Baro.Pressure_kpa > 1.0f) && (Baro.Pressure_kpa < 200.0f) &&
        (Baro.Alt_meter > -1000.0f) && (Baro.Alt_meter < 20000.0f))
    {
        snprintf(buf, len, "Alt: %.0fm  P:%.1fkPa",
                 Baro.Alt_meter, Baro.Pressure_kpa);
    }
    else
    {
        snprintf(buf, len, "Alt: --m  P:--kPa");
    }
}

static void GetMQStr(char *buf, int len)
{
#if ENABLE_BSP_MQX
    extern float g_mq2_ppm;
    extern float g_mq4_ppm;
    snprintf(buf, len, "MQ2:%-4.0fppm MQ4:%-4.0fppm",
             g_mq2_ppm, g_mq4_ppm);
#else
    snprintf(buf, len, "MQ2:--  MQ4:--");
#endif
}

static void GetGeigerStr(char *buf, int len)
{
#if ENABLE_BSP_GEIGER
    snprintf(buf, len, "Geiger:%4lucpm  %.3fuSv/h",
             Geiger_GetCPM(), Geiger_GetuSv());
#else
    snprintf(buf, len, "Geiger: --");
#endif
}

static void GetBatteryStr(char *buf, int len)
{
    int bars;
    char icon[7];

    bars = (g_battery_percent + 12) / 25;
    if (bars < 0) bars = 0;
    if (bars > 4) bars = 4;

    icon[0] = '[';
    icon[1] = (bars >= 1) ? '=' : ' ';
    icon[2] = (bars >= 2) ? '=' : ' ';
    icon[3] = (bars >= 3) ? '=' : ' ';
    icon[4] = (bars >= 4) ? '=' : ' ';
    icon[5] = ']';
    icon[6] = '\0';

    if (g_charge_active)
    {
        snprintf(buf, len, "Batt:%s+ %.2fV %3u%% CHG",
                 icon, g_battery_voltage, g_battery_percent);
    }
    else
    {
        snprintf(buf, len, "Batt:%s  %.2fV %3u%%",
                 icon, g_battery_voltage, g_battery_percent);
    }
}

static UG_COLOR GetBatteryColor(void)
{
    if (g_charge_active)
    {
        return C_CYAN;
    }
    if (g_battery_percent <= 15U)
    {
        return C_RED;
    }
    if (g_battery_percent <= 35U)
    {
        return C_YELLOW;
    }
    return C_GREEN;
}

static uint8_t GetRemoteGpsDelta(float *east_m, float *north_m, float *distance_m)
{
#if ENABLE_BSP_NEO_M8P && ENABLE_BSP_NRF24L01
    uint32_t now = HAL_GetTick();
    double local_lat_deg;
    double local_lon_deg;
    double remote_lat_deg;
    double remote_lon_deg;
    double mean_lat_rad;
    double east;
    double north;

    if ((east_m == NULL) || (north_m == NULL) || (distance_m == NULL))
    {
        return 0;
    }

    if (!(gps_position.data_valid && gps_position.fix_type >= 2))
    {
        return 0;
    }

    if (!(g_nrf24_rx_gps.frame_valid && g_nrf24_rx_gps.data_valid))
    {
        return 0;
    }

    if ((now - g_nrf24_rx_gps.tick_ms) >= 5000U)
    {
        return 0;
    }

    local_lat_deg = gps_position.lat / 10000000.0;
    local_lon_deg = gps_position.lon / 10000000.0;
    remote_lat_deg = g_nrf24_rx_gps.lat / 10000000.0;
    remote_lon_deg = g_nrf24_rx_gps.lon / 10000000.0;

    mean_lat_rad = ((local_lat_deg + remote_lat_deg) * 0.5) * (3.14159265358979323846 / 180.0);
    north = (remote_lat_deg - local_lat_deg) * 111320.0;
    east  = (remote_lon_deg - local_lon_deg) * (111320.0 * cos(mean_lat_rad));

    *east_m = (float)east;
    *north_m = (float)north;
    *distance_m = (float)sqrt((east * east) + (north * north));
    return 1;
#else
    (void)east_m;
    (void)north_m;
    (void)distance_m;
    return 0;
#endif
}

#if ENABLE_BSP_NEO_M8P
static void GetGPSStr(char *buf, int len)
{
    if (gps_position.data_valid && gps_position.fix_type >= 2)
    {
        double lat = gps_position.lat / 10000000.0;
        double lon = gps_position.lon / 10000000.0;

        snprintf(buf, len, "GPS: %.5f,%.5f", lat, lon);
    }
    else
    {
        snprintf(buf, len, "GPS: No Fix (Sats:%d)", gps_position.satellites);
    }
}
#else
static void GetGPSStr(char *buf, int len)
{
#if ENABLE_BSP_NRF24L01
    uint32_t now = HAL_GetTick();

    if (g_nrf24_rx_gps.data_valid &&
        ((now - g_nrf24_rx_gps.tick_ms) < 5000U))
    {
        double lat = g_nrf24_rx_gps.lat / 10000000.0;
        double lon = g_nrf24_rx_gps.lon / 10000000.0;

        snprintf(buf, len, "RX: %.5f,%.5f %02u:%02u:%02u",
                 lat, lon,
                 g_nrf24_rx_gps.hour,
                 g_nrf24_rx_gps.minute,
                 g_nrf24_rx_gps.second);
    }
    else
#endif
    {
        snprintf(buf, len, "GPS: --");
    }
}
#endif

static void GetRelativeGpsStr(char *buf, int len)
{
    float east_m;
    float north_m;
    float distance_m;

    if (GetRemoteGpsDelta(&east_m, &north_m, &distance_m))
    {
        snprintf(buf, len, "Rel: E:%+.0fm N:%+.0fm D:%.0fm",
                 east_m, north_m, distance_m);
    }
    else
    {
#if ENABLE_BSP_NRF24L01
        uint32_t now = HAL_GetTick();

        if (g_nrf24_rx_gps.frame_valid &&
            ((now - g_nrf24_rx_gps.tick_ms) < 5000U) &&
            !g_nrf24_rx_gps.data_valid)
        {
            snprintf(buf, len, "Rel: Remote GPS invalid");
            return;
        }
#endif
        snprintf(buf, len, "Rel: --");
    }
}

/* ========================================================================
 * DrawMenuItem — render ONE menu item: fill bg (blue/black) + text
 * ======================================================================== */
static void DrawMenuItem(UG_S16 menu_y, uint8_t idx)
{
    UG_S16 iy = (UG_S16)(menu_y + idx * 28);
    UG_S16 iw = 220;
    char buf[48];

    if (idx == menu_cursor)
    {
        UG_FillFrame(10, iy, (UG_S16)(10 + iw), (UG_S16)(iy + 20), C_BLUE);
        UG_SetForecolor(C_WHITE);
        UG_SetBackcolor(C_BLUE);
    }
    else
    {
        UG_FillFrame(10, iy, (UG_S16)(10 + iw), (UG_S16)(iy + 20), C_BLACK);
        UG_SetForecolor(C_LIGHT_GRAY);
        UG_SetBackcolor(C_BLACK);
    }

    if (idx == MENU_INTERCOM)
        snprintf(buf, sizeof(buf), "  %s: [ %s ]",
                 menu_labels[idx], intercom_on ? "ON " : "OFF");
    else if(idx == MENU_USB_MSC)
		{
			snprintf(buf, sizeof(buf), "  %s: [ %s]", menu_labels[idx], Storage_IsUsbMscMode() ? "ON":"OFF");
		}
		else
		{
			snprintf(buf, sizeof(buf)," %s",menu_labels[idx]);

    
		}
		UG_PutString(14, (UG_S16)(iy + 3), buf);
}

/* ========================================================================
 * Render: PAGE_MAIN — info + menu
 * ======================================================================== */
static void RenderPageMain(uint8_t full_redraw)
{
    char buf[64];
    UG_S16 y;
    UG_S16 info_h = 164;  /* Info area height (8 rows) */
    UG_S16 menu_y;        /* Menu section start */

    /* Page transition: targeted background fills cover thermal-image residue
     * in gaps between elements. Happens ONCE per page entry, not every frame. */
    if (full_redraw)
    {
        /* Fill gaps between title bar and separator (info bar backgrounds) */
        UG_FillFrame(0, 16, LCD_WIDTH - 1, (UG_S16)(info_h - 1), C_BLACK);
    }

    /* ----- 1. Top title bar ----- */
    UG_FontSelect(&FONT_8X12);
    DrawBar(0, 0, LCD_WIDTH, 16, C_BLUE, C_WHITE, "  Multi-Detector");

    /* ----- 2. Sensor info area ----- */
    UG_FontSelect(&FONT_6X8);
    y = 20;

    GetIRStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_GREEN, buf);

    y = 36;
    GetSHT30Str(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_CYAN, buf);

    y = 52;
    GetBaroStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_YELLOW, buf);

    y = 68;
    GetMQStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_ORANGE, buf);

    y = 84;
    GetGeigerStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_WHITE, buf);

    y = 100;
    GetBatteryStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, GetBatteryColor(), buf);

    y = 116;
    GetGPSStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_MAGENTA, buf);

    y = 132;
    GetRelativeGpsStr(buf, sizeof(buf));
    DrawBar(0, y, LCD_WIDTH, 14, C_BLACK, C_LIGHT_GRAY, buf);

    /* ----- 3. Separator ----- */
    UG_FillFrame(0, info_h, LCD_WIDTH - 1, info_h + 2, C_GRAY);

    /* ----- 4. Menu items — differential update (only changed items) ----- */
    {
        static uint8_t last_cursor   = 0xFF;  /* Force first render       */
        static uint8_t last_intercom = 0xFF;
        static uint8_t last_usb_msc  = 0xFF;
			  uint8_t usb_msc_on = Storage_IsUsbMscMode();
        uint8_t draw_all = full_redraw || (last_cursor == 0xFF);

        UG_FontSelect(&FONT_8X12);
        menu_y = (UG_S16)(info_h + 10);

        if (full_redraw)
        {
            /* Fill menu area background: gaps, margins, below last item */
            UG_FillFrame(0, (UG_S16)(info_h + 3), LCD_WIDTH - 1,
                         LCD_HEIGHT - 1, C_BLACK);
        }

        if (draw_all)
        {
            uint8_t j;
            for (j = 0; j < MENU_ITEMS; j++)
                DrawMenuItem(menu_y, j);

            /* Bottom hints — once per full redraw */
            UG_FontSelect(&FONT_5X8);
            UG_SetForecolor(C_GRAY);
            UG_SetBackcolor(C_BLACK);
            UG_PutString(4, LCD_HEIGHT - 10,
                         (char*)"[UP/DOWN]Nav  [ENTER]Select");
        }
        else
        {
            if (menu_cursor != last_cursor)
            {
                DrawMenuItem(menu_y, last_cursor);   /* Old → un-highlight */
                DrawMenuItem(menu_y, menu_cursor);   /* New → highlight   */
            }
            if ((uint8_t)intercom_on != last_intercom)
            {
                DrawMenuItem(menu_y, MENU_INTERCOM);
            }
						if (usb_msc_on != last_usb_msc)
            {
                DrawMenuItem(menu_y, MENU_USB_MSC);
            }
        }

        last_cursor   = menu_cursor;
        last_intercom = (uint8_t)intercom_on;
				last_usb_msc  = usb_msc_on;
    }
}

/* ========================================================================
 * Render: PAGE_THERMAL — minimal overlay on thermal image
 * ======================================================================== */
static void RenderPageThermal(void)
{
    char buf[48];

    /* Top bar: title + instruction */
    UG_FontSelect(&FONT_6X8);
    DrawBar(0, 0, LCD_WIDTH, 14, C_BLACK, C_WHITE,
            " Thermal View  [RETURN]Back");

    /* Bottom bar: IR temperature info */
    GetIRStr(buf, sizeof(buf));
    DrawBar(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, C_BLACK, C_GREEN, buf);
}

/* ========================================================================
 * Render send status popup (overlay on any page)
 * ======================================================================== */
static void RenderSendStatus(void)
{
    UG_S16 sx = 50, sy = 110;
    const char *msg;
    UG_COLOR   color;
    static uint8_t prev_status = 0;

    if (send_status == 1)       { msg = "Sending location..."; color = C_YELLOW; }
    else if (send_status == 2)  { msg = "Location sent!";      color = C_GREEN;  }
    else if (send_status == 3)  { msg = "Send failed!";        color = C_RED;    }
    else
    {
        /* Popup just disappeared — clear its rectangle */
        if (prev_status != 0)
            UG_FillFrame(sx, sy, (UG_S16)(sx + 220), (UG_S16)(sy + 26), C_BLACK);
        prev_status = 0;
        return;
    }

    UG_FontSelect(&FONT_8X12);
    UG_SetForecolor(color);
    UG_SetBackcolor(C_BLACK);
    UG_FillFrame(sx, sy, (UG_S16)(sx + 220), (UG_S16)(sy + 26), C_BLACK);
    UG_PutString(sx + 20, sy + 6, (char*)msg);

    prev_status = send_status;   /* Remember for next frame's cleanup check */

    send_timer++;
    if (send_timer > 6)
    {
        send_status = 0;
        send_timer  = 0;
    }
}

/* ========================================================================
 * Key handlers
 * ======================================================================== */

static void HandleKeyMain(uint8_t key)
{
    switch (key)
    {
    case KEY_UP:
        if (menu_cursor > 0)
            menu_cursor--;
        else
            menu_cursor = MENU_ITEMS - 1;
        break;

    case KEY_DOWN:
        if (menu_cursor < MENU_ITEMS - 1)
            menu_cursor++;
        else
            menu_cursor = 0;
        break;

    case KEY_ENTER:
        switch (menu_cursor)
        {
        case MENU_THERMAL:
            current_page = PAGE_THERMAL;
            break;

        case MENU_INTERCOM:
            intercom_on = !intercom_on;
#if ENABLE_BSP_EWM201
            if (intercom_on)
            {
                EWM201_PowerDown(0);
            }
            else             
            {
                EWM201_PowerDown(1);
            }
#endif
            break;

        case MENU_SEND_LOC:
            send_status = 1;
            send_timer  = 0;
            break;

        case MENU_USB_MSC:
					     if (!Storage_IsBusy())
    {
        if (Storage_IsUsbMscMode())
        {
            Storage_RequestUsbMscOff();
        }
        else
        {
            Storage_RequestUsbMscOn();
        }
    }
            /* TODO: show version / credits */
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
}

static void HandleKeyThermal(uint8_t key)
{
    if (key == KEY_RETURN)
        current_page = PAGE_MAIN;
}

/* ========================================================================
 * Send location — background action
 * ======================================================================== */
static void ProcessSendLocation(void)
{
    uint8_t ok = 0;

    if (send_status != 1)
        return;

#if ENABLE_BSP_EWM201
    if (!intercom_on)
        EWM201_Init();
#endif

#if ENABLE_BSP_NRF24L01
    {
#if ENABLE_BSP_NEO_M8P
        if (gps_position.data_valid && gps_position.time_valid && gps_position.fix_type >= 2)
        {
            ok = NRF24_SendGpsLocation(&gps_position);
        }
        else
#endif
        {
            GPS_Position_t invalid_pos = {0};
            ok = NRF24_SendGpsLocation(&invalid_pos);
        }
    }
#endif

    send_status = ok ? 2 : 3;
    send_timer  = 0;
}
/* ========================================================================
 * GUI_AppTask — called each frame after UG_Update()
 * ======================================================================== */
void GUI_AppTask(void)
{
    uint8_t key;

    key = KEY_GetEvent();

    if (key != KEY_NONE)
    {
        switch (current_page)
        {
        case PAGE_MAIN:    HandleKeyMain(key);    break;
        case PAGE_THERMAL: HandleKeyThermal(key); break;
        default: break;
        }
    }

    /* 3. Background tasks */
#if ENABLE_BSP_NRF24L01
    if (send_status != 1)
    {
        NRF24_ReceiveGpsLocation();
    }
#endif
    ProcessSendLocation();

    {
        static uint8_t prev_page = 0xFF;
        uint8_t page_changed = (current_page != prev_page);
        prev_page = current_page;

        switch (current_page)
        {
        case PAGE_MAIN:
            RenderPageMain(page_changed);
            break;
        case PAGE_THERMAL:
            RenderPageThermal();
            break;
        default:
            break;
        }
    }

    RenderSendStatus();
}
