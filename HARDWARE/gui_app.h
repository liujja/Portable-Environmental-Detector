/**
 ****************************************************************************************************
 * @file        gui_app.h
 * @brief       GUI application layer ¡ª menu system, sensor data display
 * @note        Key-driven navigation, UGUI-based rendering
 ****************************************************************************************************
 */

#ifndef __GUI_APP_H
#define __GUI_APP_H

#include "sys_task.h"

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void    GUI_AppInit(void);         /* Create UGUI windows & objects       */
void    GUI_AppTask(void);         /* Process keys + update display       */
uint8_t GUI_IsThermalActive(void); /* Returns 1 if thermal page is active  */

#endif /* __GUI_APP_H */
