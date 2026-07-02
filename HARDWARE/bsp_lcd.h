#ifndef _BSP_LCD_H
#define _BSP_LCD_H

#include "sys_task.h"

#define LCD_RST_GPIO_PORT           GPIOH
#define LCD_RST_GPIO_PIN            GPIO_PIN_10
#define LCD_RST_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOH_CLK_ENABLE();  //复位端

#define LCD_RD_GPIO_PORT          GPIOH
#define LCD_RD_GPIO_PIN           GPIO_PIN_9
#define LCD_RD_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOH_CLK_ENABLE();  //读控制端

#define LCD_WR_GPIO_PORT           GPIOH
#define LCD_WR_GPIO_PIN            GPIO_PIN_8
#define LCD_WR_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOH_CLK_ENABLE();  //写控制端

#define LCD_RS_GPIO_PORT           GPIOH
#define LCD_RS_GPIO_PIN            GPIO_PIN_7
#define LCD_RS_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOH_CLK_ENABLE();  //命令/数据切换

#define LCD_CS_GPIO_PORT           GPIOH
#define LCD_CS_GPIO_PIN            GPIO_PIN_6
#define LCD_CS_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOH_CLK_ENABLE();  //片选

#define LCD_BL_GPIO_PORT           GPIOH
#define LCD_BL_GPIO_PIN            GPIO_PIN_11
#define LCD_BL_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOH_CLK_ENABLE(); //背光控制端

#define DATA_PORT                  GPIOE
#define DATA_PORT_CLK_ENABLE()     __HAL_RCC_GPIOE_CLK_ENABLE();  //数据端
//8位并口 PE0-PE7  D0-D7
#define LCD_D0_PIN                 GPIO_PIN_0
#define LCD_D1_PIN                 GPIO_PIN_1
#define LCD_D2_PIN                 GPIO_PIN_2
#define LCD_D3_PIN                 GPIO_PIN_3
#define LCD_D4_PIN                 GPIO_PIN_4
#define LCD_D5_PIN                 GPIO_PIN_5
#define LCD_D6_PIN                 GPIO_PIN_6
#define LCD_D7_PIN                 GPIO_PIN_7

#define LCD_BL(x)   do{ x ? \
                      HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0) 
#define LCD_RS(x)   do{ x ? \
                      HAL_GPIO_WritePin(LCD_RS_GPIO_PORT, LCD_RS_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LCD_RS_GPIO_PORT, LCD_RS_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)
#define LCD_CS(x)   do{ x ? \
                      HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)
#define LCD_WR(x)   do{ x ? \
                      HAL_GPIO_WritePin(LCD_WR_GPIO_PORT, LCD_WR_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LCD_WR_GPIO_PORT, LCD_WR_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)
#define LCD_RD(x)   do{ x ? \
                      HAL_GPIO_WritePin(LCD_RD_GPIO_PORT, LCD_RD_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LCD_RD_GPIO_PORT, LCD_RD_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)
#define LCD_RST(x)  do{ x ? \
                      HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)

////*************  16位色定义 *************//
#define White          0xFFFF
#define Black          0x0000
#define Blue           0x001F
#define Blue2          0x051F
#define Red            0xF800
#define Magenta        0xF81F
#define Green          0x07E0
#define Cyan           0x7FFF
#define Yellow         0xFFE0

/* LCD physical resolution — TK018F14LV */
#define LCD_WIDTH   240
#define LCD_HEIGHT  320

void LCD_Initial(void);
void LCD_GPIO_Config(void);
void WriteComm(uint16_t CMD);
void LCD_WriteData(uint16_t data);
void Lcd_ColorBox(uint16_t xStart,uint16_t yStart,uint16_t xLong,uint16_t yLong,uint32_t Color);
void BlockWrite(unsigned int Xstart, unsigned int Xend, unsigned int Ystart, unsigned int Yend);
void LCD_PutString(unsigned short x, unsigned short y, char *s, unsigned int fColor, unsigned int bColor,unsigned char flag);
void DrawPixel(uint16_t x, uint16_t y, int Color);
void display_task(void *pvParameters);

#if ENABLE_BSP_MLX90640
void LCD_ShowThermalImage(void);
#endif



#endif

