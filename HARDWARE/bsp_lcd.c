/**
 ****************************************************************************************************
 * @file        bsp_lcd.c
 * @brief       LCD Board Support Package — 8080 parallel (TK018F14LV 320×240)
 * @note        基于 reference_word/LCD_20260605/LCD.c 适配 STM32H7 HAL 库
 ****************************************************************************************************
 */

#include "bsp_lcd.h"
#include "stm32h7xx_hal.h"
/* ========================================================================
 * Font data (HARDWARE/ASCII.h, HARDWARE/GB1616.h)
 * ======================================================================== */
#include "ASCII.h"
#include "GB1616.h"

/* ========================================================================
 * Shared: MLX90640 temperature array (declared in bsp_mlx90640.h)
 * ======================================================================== */
#if ENABLE_BSP_MLX90640
extern float mlx90640_temperatures[768];
#endif

/* ========================================================================
 * 8080 PARALLEL LCD — TK018F14LV 320×240
 * ======================================================================== */
#if ENABLE_BSP_8080_LCD

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "delay.h"


/* ========================================================================
 * Low-level 8080 bus primitives (16-bit protocol over 8-bit bus)
 * ======================================================================== */

/**
 * @brief  发送 16-bit 命令 — 高字节先发, 低字节后发
 */
void WriteComm(uint16_t CMD)
{
    LCD_CS(0);
    LCD_RS(0);                          /* RS=0 → 命令模式 */
    GPIOE->ODR = (uint8_t)(CMD >> 8);   /* 高 8 位 */
    LCD_WR(0); LCD_WR(1);
    GPIOE->ODR = (uint8_t)(CMD);        /* 低 8 位 */
    LCD_WR(0); LCD_WR(1);
    LCD_CS(1);
}

/**
 * @brief  发送 16-bit 数据 — 高字节先发, 低字节后发
 */
void LCD_WriteData(uint16_t dat)
{
    LCD_CS(0);
    LCD_RS(1);                          /* RS=1 → 数据模式 */
    GPIOE->ODR = (uint8_t)(dat >> 8);   /* 高 8 位 */
    LCD_WR(0); LCD_WR(1);
    GPIOE->ODR = (uint8_t)(dat);        /* 低 8 位 */
    LCD_WR(0); LCD_WR(1);
    LCD_CS(1);
}

/* ========================================================================
 * GPIO 初始化 — 8080 并口引脚配置
 * ======================================================================== */

void LCD_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_Initure.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull  = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;

    /* CS=PH6, RS=PH7, WR=PH8, RD=PH9, RST=PH10, BL=PH11 */
    GPIO_Initure.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8
                     | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOH, &GPIO_Initure);

    /* D0-D7 = PE0-PE7 (8-bit data bus) */
    GPIO_Initure.Pin = GPIO_PIN_All;
    HAL_GPIO_Init(GPIOE, &GPIO_Initure);

    /* 设置控制引脚初始电平 (参考 TK499 例程: RD=1 防止总线冲突) */
    LCD_CS(1);   /* CS  = HIGH (未选中) */
    LCD_RS(1);   /* RS  = HIGH (数据模式) */
    LCD_WR(1);   /* WR  = HIGH (写无效) */
    LCD_RD(1);   /* RD  = HIGH (读无效 — 关键! 低电平会导致总线冲突) */
    LCD_RST(1);  /* RST = HIGH (正常运行) */
    LCD_BL(0);   /* BL  = LOW  (背光先关闭, 初始化后再打开) */
}

static void LCD_Reset(void)
{
    LCD_RST(0);
    delayms(1000);
    LCD_RST(1);
    delayms(1000);
}

/* ========================================================================
 * 窗口 & 像素操作 — TK018F14LV 寄存器映射
 * ======================================================================== */

/**
 * @brief  设置写入窗口 (开窗)
 * @note   寄存器: 0x0406(H_Start) 0x0407(H_End) 0x0408(V_Start) 0x0409(V_End)
 *         起始地址: 0x0200(Y_addr) 0x0201(X_addr)
 *         RAM 写入: 0x0202
 */
void BlockWrite(unsigned int Xstart, unsigned int Xend,
                unsigned int Ystart, unsigned int Yend)
{
    unsigned int H_start = 239 - Xend;
    unsigned int H_end   = 239 - Xstart;

    /* Portrait orientation: H controls X(0..239), V controls Y(0..319) */
    WriteComm(0x0406); LCD_WriteData(H_start);  /* H window start */
    WriteComm(0x0407); LCD_WriteData(H_end);    /* H window end   */
    WriteComm(0x0408); LCD_WriteData(Ystart);   /* V window start */
    WriteComm(0x0409); LCD_WriteData(Yend);     /* V window end   */

    WriteComm(0x0200); LCD_WriteData(239 - Xstart); /* H start address (Logical Left) */
    WriteComm(0x0201); LCD_WriteData(Ystart);       /* V start address */
    WriteComm(0x0202);                          /* RAM write */
}

/**
 * @brief  矩形填充 — 快速模式 (直接操作 ODR + WR 脉冲)
 */
void Lcd_ColorBox(uint16_t xStart, uint16_t yStart,
                  uint16_t xLong, uint16_t yLong, uint32_t Color)
{
    uint32_t temp;

    BlockWrite(xStart, xStart + xLong - 1, yStart, yStart + yLong - 1);

    LCD_CS(0);
    LCD_RS(1);  /* Data mode */
    for (temp = 0; temp < (uint32_t)xLong * yLong; temp++)
    {
        GPIOE->ODR = (uint8_t)(Color >> 8);   /* 高 8 位 */
        LCD_WR(0); LCD_WR(1);
        GPIOE->ODR = (uint8_t)(Color);        /* 低 8 位 */
        LCD_WR(0); LCD_WR(1);
    }
    LCD_CS(1);
}

/**
 * @brief  在 (x, y) 坐标打点
 */
void DrawPixel(uint16_t x, uint16_t y, int Color)
{
    WriteComm(0x0200);
    LCD_WriteData(239 - x);     /* H 地址 */
    WriteComm(0x0201);
    LCD_WriteData(y);           /* V 地址 */
    WriteComm(0x0202);          /* 启动 RAM 写入 */

    LCD_CS(0);
    LCD_RS(1);                  /* Data mode */
    GPIOE->ODR = (uint8_t)(Color >> 8);
    LCD_WR(0); LCD_WR(1);
    GPIOE->ODR = (uint8_t)(Color);
    LCD_WR(0); LCD_WR(1);
    LCD_CS(1);
}

/* ========================================================================
 * LCD 初始化序列 — TK018F14LV
 * ======================================================================== */

void LCD_Initial(void)
{
    LCD_GPIO_Config();
    LCD_Reset();

    LCD_BL(1);          /* Backlight ON */
    LCD_CS(0);          /* Chip select ON */

    /* --- 1. Exit deep standby (×3, 10ms each) --- */
    WriteComm(0x0000);  delayms(10);
    WriteComm(0x0000);  delayms(10);
    WriteComm(0x0000);  delayms(10);

    /* --- 2. Mode setting: exit standby --- */
    WriteComm(0x05FF);  LCD_WriteData(0x0000);
    WriteComm(0x001D);  LCD_WriteData(0x0005);
    delayms(100);

    /* --- 3. Oscillation ON --- */
    WriteComm(0x0000);  LCD_WriteData(0x0001);
    delayms(100);

    /* --- 4. Display control --- */
    WriteComm(0x0001);  LCD_WriteData(0x0027);   /* Driver output, SS=0 (Revert shift) */
    WriteComm(0x0002);  LCD_WriteData(0x0200);   /* AC line inversion */
    WriteComm(0x0003);  LCD_WriteData(0x0038);   /* Entry mode (ref value) */
    WriteComm(0x0007);  LCD_WriteData(0x4004);   /* 262K colors, normal drive */
    WriteComm(0x000D);  LCD_WriteData(0x0011);   /* FR period 60Hz */
    delayms(100);

    /* --- 5. LTPS control --- */
    WriteComm(0x0012);  LCD_WriteData(0x0303);
    WriteComm(0x0013);  LCD_WriteData(0x0102);
    WriteComm(0x001C);  LCD_WriteData(0x0000);

    /* --- 6. Power settings --- */
    WriteComm(0x0102);  LCD_WriteData(0x00F6);   /* VCOM */
    delayms(500);
    WriteComm(0x0103);  LCD_WriteData(0x0007);   /* XVDD */
    delayms(100);
    WriteComm(0x0105);  LCD_WriteData(0x0111);
    delayms(100);

    /* --- 7. Gamma correction --- */
    WriteComm(0x0300);  LCD_WriteData(0x0200);
    WriteComm(0x0301);  LCD_WriteData(0x0002);
    WriteComm(0x0302);  LCD_WriteData(0x0000);
    WriteComm(0x0303);  LCD_WriteData(0x0300);
    WriteComm(0x0304);  LCD_WriteData(0x0700);
    WriteComm(0x0305);  LCD_WriteData(0x0070);

    /* --- 8. Window setup (first screen & active window) --- */
    WriteComm(0x0402);  LCD_WriteData(0x0000);   /* First screen start = 0 */
    WriteComm(0x0403);  LCD_WriteData(0x013F);   /* First screen end   = 319 */

    /* Portrait AM=0: H->X(240), V->Y(320); origin=(0,0) top-left */
    WriteComm(0x0406);  LCD_WriteData(0x0000);   /* H window start = 0 */
    WriteComm(0x0407);  LCD_WriteData(0x00EF);   /* H window end   = 239 */
    WriteComm(0x0408);  LCD_WriteData(0x0000);   /* V window start = 0 */
    WriteComm(0x0409);  LCD_WriteData(0x013F);   /* V window end   = 319 */

    WriteComm(0x0200);  LCD_WriteData(0x0000);   /* H start = 0 */
    WriteComm(0x0201);  LCD_WriteData(0x0000);   /* V start = 0 */

    /* --- 9. Display ON sequence --- */
    WriteComm(0x0100);  LCD_WriteData(0xC010);
    delayms(500);
    WriteComm(0x0101);  LCD_WriteData(0x0001);
    WriteComm(0x0100);  LCD_WriteData(0xF7FE);
    delayms(800);

    /* I/D=01(H dec, V inc), AM=0 → Portrait Mode, Physical X mirrored */
    WriteComm(0x0003);  LCD_WriteData(0x0010);

    /* --- 11. Clear screen to black --- */
    Lcd_ColorBox(0, 0, LCD_WIDTH, LCD_HEIGHT, Black);
}

/* ========================================================================
 * Font rendering functions (ASCII 8×16 + GB2312 16×16)
 * ======================================================================== */

void SPILCD_ShowChar(unsigned short x, unsigned short y, unsigned char num,
                     unsigned int fColor, unsigned int bColor, unsigned char flag)
{
    unsigned char temp;
    unsigned int pos, i, j;

    num = num - ' ';            /* Offset from space */
    i = num * 16;
    for (pos = 0; pos < 16; pos++)
    {
        temp = nAsciiDot[i + pos];
        for (j = 0; j < 8; j++)
        {
            if (temp & 0x80)
                DrawPixel(x + j, y, fColor);
            else if (flag)
                DrawPixel(x + j, y, bColor);
            temp <<= 1;
        }
        y++;
    }
}

void PutGB1616(unsigned short x, unsigned short y, unsigned char c[2],
               unsigned int fColor, unsigned int bColor, unsigned char flag)
{
    unsigned int i, j, k;
    unsigned short m;

    for (k = 0; k < 64; k++)
    {
        if ((codeGB_16[k].Index[0] == c[0]) && (codeGB_16[k].Index[1] == c[1]))
        {
            for (i = 0; i < 32; i++)
            {
                m = codeGB_16[k].Msk[i];
                for (j = 0; j < 8; j++)
                {
                    if ((m & 0x80) == 0x80)
                        DrawPixel(x + j, y, fColor);
                    else if (flag)
                        DrawPixel(x + j, y, bColor);
                    m <<= 1;
                }
                if (i % 2) { y++; x = x - 8; }
                else       { x = x + 8; }
            }
        }
    }
}

void LCD_PutString(unsigned short x, unsigned short y, char *s,
                   unsigned int fColor, unsigned int bColor, unsigned char flag)
{
    unsigned char l = 0;
    while (*s)
    {
        if ((unsigned char)*s < 0x80)
        {
            SPILCD_ShowChar(x + l * 8, y, *s, fColor, bColor, flag);
            s++; l++;
        }
        else
        {
            PutGB1616(x + l * 8, y, (unsigned char *)s, fColor, bColor, flag);
            s += 2; l += 2;
        }
    }
}

/* ========================================================================
 * Ironbow False-Color Mapping (Temperature → RGB565)
 * ======================================================================== */

#if ENABLE_BSP_MLX90640
static uint16_t GetTemperatureColor(float val, float min_val, float max_val)
{
    float norm = (val - min_val) / (max_val - min_val);
    uint8_t r = 0, g = 0, b = 0;

    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    if (norm < 0.25f)
    {
        b = 255;
        g = (uint8_t)(norm * 4.0f * 255);
    }
    else if (norm < 0.5f)
    {
        g = 255;
        b = (uint8_t)(255 - (norm - 0.25f) * 4.0f * 255);
    }
    else if (norm < 0.75f)
    {
        g = 255;
        r = (uint8_t)((norm - 0.5f) * 4.0f * 255);
    }
    else
    {
        r = 255;
        g = (uint8_t)(255 - (norm - 0.75f) * 4.0f * 255);
    }
    return (((uint16_t)(r >> 3)) << 11) |
           (((uint16_t)(g >> 2)) << 5)  |
           ((uint16_t)(b >> 3));
}
#endif /* ENABLE_BSP_MLX90640 */

/* ========================================================================
 * LCD_ShowThermalImage — 渲染一帧热成像画面 (320×240 full screen)
 * ======================================================================== */
#if ENABLE_BSP_MLX90640

/**
 * @brief  单帧热成像渲染: dynamic range → false-color → full-screen pixels → text overlay
 * @note   调用前需确保 LCD 已初始化, 每帧调用一次, ~4Hz 时配合 vTaskDelay(250ms)
 */
void LCD_ShowThermalImage(void)
{
    int i, j, repeatX, repeatY;
    float min_t, max_t;
    char lcd_buf[64];
    uint16_t colorMap[24][32];

    /* 1. Dynamic range: find min/max in current frame */
    min_t = 100.0f;
    max_t = -100.0f;
    for (i = 0; i < 768; i++)
    {
        if (mlx90640_temperatures[i] > 0.1f ||
            mlx90640_temperatures[i] < -0.1f)
        {
            if (mlx90640_temperatures[i] < min_t)
                min_t = mlx90640_temperatures[i];
            if (mlx90640_temperatures[i] > max_t)
                max_t = mlx90640_temperatures[i];
        }
    }
    if (min_t > max_t) { min_t = 25.0f; max_t = 26.0f; }
    if (max_t - min_t < 0.1f) max_t = min_t + 0.1f;

    /* 2. Full-screen thermal: CCW 90 + LR Mirror scaled layout */
    BlockWrite(0, LCD_WIDTH - 1, 0, LCD_HEIGHT - 1);
    LCD_CS(0);
    LCD_RS(1);  /* Data mode */

    /* Pre-compute full 24x32 color map to optimize rendering speed */
    for (i = 0; i < 24; i++)
    {
        for (j = 0; j < 32; j++)
        {
            float current_val = mlx90640_temperatures[i * 32 + j];

            /* Horizontal interpolation for checkerboard pattern missing cells */
            if (current_val > -0.001f && current_val < 0.001f)
            {
                if (j == 0)
                    current_val = mlx90640_temperatures[i * 32 + 1];
                else if (j == 31)
                    current_val = mlx90640_temperatures[i * 32 + 30];
                else
                    current_val = (mlx90640_temperatures[i * 32 + j - 1] +
                                   mlx90640_temperatures[i * 32 + j + 1]) / 2.0f;
            }
            colorMap[i][j] = GetTemperatureColor(current_val, min_t, max_t);
        }
    }

    /* Render dynamically scaled pixels mapped to match final desired orientation */
    for (repeatY = 0; repeatY < LCD_HEIGHT; repeatY++)
    {
        /* CCW 90 + LR Mirror: Y axis maps to c (0..31) */
        int c = 31 - (repeatY / 10);
        for (repeatX = 0; repeatX < LCD_WIDTH; repeatX++)
        {
            /* X axis maps to r (0..23) */
            int r = 23 - (repeatX / 10);
            uint16_t color = colorMap[r][c];

            GPIOE->ODR = (uint8_t)(color >> 8);
            LCD_WR(0); LCD_WR(1);
            GPIOE->ODR = (uint8_t)(color);
            LCD_WR(0); LCD_WR(1);
        }
    }
    LCD_CS(1);

    /* 3. Temperature text overlay (top-left, black background) */
    snprintf(lcd_buf, sizeof(lcd_buf), "Min:%.1f Max:%.1f", min_t, max_t);
    LCD_PutString(2, 2, lcd_buf, White, Black, 1);
}

#endif /* ENABLE_BSP_MLX90640 */

/* ========================================================================
 * display_task — LCD 显示任务
 *   - UGUI 模式 (ENABLE_BSP_UGUI=1): 热成像 + GUI 叠加
 *   - 裸驱动模式 (ENABLE_BSP_UGUI=0): 纯热成像渲染
 * ======================================================================== */

#if ENABLE_BSP_UGUI

#include "ugui_port.h"
#include "gui_app.h"

void display_task(void *pvParameters)
{
    /* Clear screen */
    Lcd_ColorBox(0, 0, LCD_WIDTH, LCD_HEIGHT, Black);

    /* Initialize UGUI */
    UG_PortInit();

    /* Initialize GUI application (windows, menus, state) */
    GUI_AppInit();

#if ENABLE_BSP_MLX90640
    while (1)
    {
        /* Show thermal image ONLY when thermal page is active */
        if (GUI_IsThermalActive())
            LCD_ShowThermalImage();

        /* Update UGUI internal state */
        UG_Update();

        /* Run GUI app: key processing + page rendering */
        GUI_AppTask();

        vTaskDelay(pdMS_TO_TICKS(250));
    }
#else
    /* No sensor: show UGUI info screen + menu */
    while (1)
    {
        UG_FillScreen(C_BLACK);
        UG_Update();
        GUI_AppTask();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#endif
}

#else /* !ENABLE_BSP_UGUI — bare-metal display */

void display_task(void *pvParameters)
{
    /* Clear screen */
    Lcd_ColorBox(0, 0, LCD_WIDTH, LCD_HEIGHT, Black);

#if ENABLE_BSP_MLX90640
    while (1)
    {
        LCD_ShowThermalImage();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
#else
    /* No MLX90640 sensor — show test card */
    Lcd_ColorBox(0, 0, LCD_WIDTH, LCD_HEIGHT, Blue);
    LCD_PutString(40, 100, "LCD Test - TK018F14LV", White, Blue, 1);
    LCD_PutString(40, 130, "320x240 8080 16bit", Yellow, Blue, 1);
    vTaskSuspend(NULL);  /* done, sleep forever */
    while (1) { vTaskDelay(1000); }
#endif
}

#endif /* ENABLE_BSP_UGUI */

#endif /* ENABLE_BSP_8080_LCD */
