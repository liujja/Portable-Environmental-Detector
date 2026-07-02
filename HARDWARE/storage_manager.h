#ifndef __STORAGE_MANAGER_H
#define __STORAGE_MANAGER_H

#include <stdint.h>
#include "cmsis_os2.h"

typedef enum
{
    STORAGE_MODE_LOCAL_LOG = 0,
    STORAGE_MODE_USB_MSC
} StorageMode_t;

typedef enum
{
    STORAGE_REQ_NONE = 0,
    STORAGE_REQ_TO_USB_MSC,
    STORAGE_REQ_TO_LOCAL_LOG
} StorageRequest_t;


#define TF_TASK_FLAG_MOUNT      (1U << 0)
#define TF_TASK_FLAG_UNMOUNT    (1U << 1)

#define USB_TASK_FLAG_START     (1U << 0)
#define USB_TASK_FLAG_STOP      (1U << 1)

extern volatile StorageMode_t g_storage_mode;
extern volatile uint8_t g_tf_log_enable;
extern volatile uint8_t g_usb_msc_started;
extern volatile uint8_t g_storage_switch_busy;

/* TF/FatFs È«¾Ö»¥³âËø */
extern osMutexId_t tfFsMutexHandle;
extern osThreadId_t tfTaskHandle;
extern osThreadId_t usbTaskHandle;

uint8_t Storage_IsUsbMscMode(void);
uint8_t Storage_IsBusy(void);

void Storage_RequestUsbMscOn(void);
void Storage_RequestUsbMscOff(void);

uint8_t Storage_IsUsbMscMode(void);
void Storage_RequestUsbMscToggle(void);
void storage_manager_task(void *argument);

#endif

