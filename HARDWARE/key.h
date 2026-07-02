/**
 ****************************************************************************************************
 * @file        key.h
 * @brief       6-key input driver ¡ª pull-up, active-low
 * @note        PB9=UP, PI4=DOWN, PI5=RETURN, PI6=RIGHT, PG13=ENTER, PG14=LEFT
 ****************************************************************************************************
 */

#ifndef __KEY_H
#define __KEY_H

#include "sys_task.h"

/* Key index enumeration */
#define KEY_UP      0
#define KEY_DOWN    1
#define KEY_RETURN  2
#define KEY_RIGHT   3
#define KEY_ENTER   4
#define KEY_LEFT    5
#define KEY_NONE    0xFF

/* Key state ¡ª active-low */
#define KEY_PRESS   0

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void    KEY_Init(void);
uint8_t KEY_Scan(void);       /* Raw edge-triggered scan (call at 10-20ms from key_task only) */
uint8_t KEY_GetEvent(void);   /* Pop buffered event from ring buffer, KEY_NONE if empty      */
void    key_task(void *pvParameters);  /* High-priority key scanning task (10ms interval)     */

#endif /* __KEY_H */
