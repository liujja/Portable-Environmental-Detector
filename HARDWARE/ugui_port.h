/**
 ****************************************************************************************************
 * @file        ugui_port.h
 * @brief       UGUI hardware port layer °™ TK018F14LV 320x240 LCD via 8080 parallel bus
 * @note          ≈‰ STM32H7 HAL + FreeRTOS
 ****************************************************************************************************
 */

#ifndef __UGUI_PORT_H
#define __UGUI_PORT_H

#include "ugui.h"

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void UG_PortInit(void);
UG_U16 UG_ColorToRGB565(UG_COLOR c);

#endif /* __UGUI_PORT_H */
