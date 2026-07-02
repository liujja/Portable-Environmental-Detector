/**
 ****************************************************************************************************
 * @file        ugui_port.c
 * @brief       UGUI hardware adaptation layer ！ bridges to bsp_lcd (TK018F14LV 320x240)
 * @note        STM32H7 + FreeRTOS: only display_task accesses UGUI, no mutex needed
 ****************************************************************************************************
 */

#include "ugui_port.h"
#include "bsp_lcd.h"

/* ========================================================================
 * Global GUI instance
 * ======================================================================== */
static UG_GUI gui;

/* ========================================================================
 * Color conversion ！ UG_COLOR (RGB565) already matches LCD format
 * ======================================================================== */

/**
 * @brief  Convert UG_COLOR to raw RGB565 (identity ！ both are 16-bit 565)
 */
UG_U16 UG_ColorToRGB565(UG_COLOR c)
{
    return (UG_U16)c;
}

/* ========================================================================
 * Pixel-set callback ！ required by UGUI
 * ======================================================================== */

/**
 * @brief  UGUI pixel drawing callback ！ delegates to BSP DrawPixel
 */
static void UG_PsetCallback(UG_S16 x, UG_S16 y, UG_COLOR c)
{
    DrawPixel((uint16_t)x, (uint16_t)y, (int)(UG_U16)c);
}

/* ========================================================================
 * Hardware acceleration drivers
 * ======================================================================== */

/**
 * @brief  HW fill-frame driver ！ uses Lcd_ColorBox for fast rectangle fill
 */
static void UG_FillFrameDriver(UG_S16 x1, UG_S16 y1,
                                UG_S16 x2, UG_S16 y2, UG_COLOR c)
{
    UG_S16 xs, ys, w, h;

    /* Normalize coordinates */
    xs = (x1 < x2) ? x1 : x2;
    ys = (y1 < y2) ? y1 : y2;
    w  = (UG_S16)(((x1 < x2) ? (x2 - x1) : (x1 - x2)) + 1);
    h  = (UG_S16)(((y1 < y2) ? (y2 - y1) : (y1 - y2)) + 1);

    Lcd_ColorBox((uint16_t)xs, (uint16_t)ys,
                 (uint16_t)w,  (uint16_t)h,
                 (uint32_t)(UG_U16)c);
}

/**
 * @brief  HW draw-line driver ！ stub (UGUI falls back to internal Bresenham)
 */
static void UG_DrawLineDriver(UG_S16 x1, UG_S16 y1,
                               UG_S16 x2, UG_S16 y2, UG_COLOR c)
{
    /* Stub: not implemented in HW ！ UGUI uses software fallback */
    (void)x1; (void)y1; (void)x2; (void)y2; (void)c;
}

/* ========================================================================
 * UGUI port initialization
 * ======================================================================== */

/**
 * @brief  Initialize UGUI: create GUI instance, register drivers, set defaults
 * @note   Call once after LCD_Initial(), before any UGUI rendering
 */
void UG_PortInit(void)
{
    /* 1. Init UGUI with pixel callback and display dimensions */
    UG_Init(&gui, UG_PsetCallback, (UG_S16)LCD_WIDTH, (UG_S16)LCD_HEIGHT);

    /* 2. Register hardware acceleration drivers */
    UG_DriverRegister(DRIVER_FILL_FRAME, (void*)UG_FillFrameDriver);
    UG_DriverRegister(DRIVER_DRAW_LINE,   (void*)UG_DrawLineDriver);

    /* 3. Set default colors */
    UG_FillScreen(C_BLACK);
    UG_SetForecolor(C_WHITE);
    UG_SetBackcolor(C_BLACK);

    /* 4. Select default font */
    UG_FontSelect(&FONT_8X12);
}
