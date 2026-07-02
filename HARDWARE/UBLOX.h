#ifndef _UBLOX_M8N_H_
#define _UBLOX_M8N_H_

#ifdef __cplusplus
extern "C" {
#endif
	
#include "sys_task.h"
#include <stdint.h>
#include <stdbool.h>

// GPS???????
#include <string.h>
#include <stdio.h>

/* GPS????? */
typedef struct {
    int32_t lat;           
    int32_t lon;           
    int32_t alt;         
    uint8_t fix_type;      // 2=2D, 3=3D, 4=DGPS, 5=RTK Float, 6=RTK Fixed
    uint8_t satellites;    
    float h_acc;           
    float v_acc;           
	  uint8_t data_valid; 

    uint8_t hour;       // UTC hour
    uint8_t minute;     // UTC minute
    uint8_t second;     // UTC second
    uint8_t time_valid; // 1=UTC time valid	
} GPS_Position_t;

extern GPS_Position_t gps_position;
extern volatile uint32_t gps_rx_byte_count;
extern volatile uint32_t gps_nmea_sentence_count;
extern volatile uint32_t gps_ubx_pvt_count;
extern volatile uint32_t gps_last_rx_tick_ms;


void GPS_Process_Byte(uint8_t data);
void GPS_Print_Position(void);
void GPS_Delay_ms(uint32_t ms);
void GPS_Init(void);

#ifdef __cplusplus
}
#endif

#endif

