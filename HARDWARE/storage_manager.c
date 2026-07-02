#include "storage_manager.h"
#include "cmsis_os.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "TF_Card.h"
#include "ff.h"
#include <stdio.h>


#ifndef STORAGE_DEFAULT_USB_MSC
#define STORAGE_DEFAULT_USB_MSC 0
#endif

extern USBD_HandleTypeDef hUsbDeviceFS;

volatile StorageMode_t g_storage_mode = STORAGE_MODE_LOCAL_LOG;
volatile uint8_t g_tf_log_enable = 0;
volatile uint8_t g_usb_msc_started = 0;
volatile uint8_t g_storage_switch_busy = 0;


static uint8_t Storage_LockFs(uint32_t timeout)
{
    return (osMutexAcquire(tfFsMutexHandle, timeout) == osOK) ? 1U : 0U;
}

static void Storage_UnlockFs(void)
{
    osMutexRelease(tfFsMutexHandle);
}

uint8_t Storage_IsUsbMscMode(void)
{
    return (g_storage_mode == STORAGE_MODE_USB_MSC) ? 1 : 0;
}

uint8_t Storage_IsBusy(void)
{
    return g_storage_switch_busy;
}

void Storage_RequestUsbMscToggle(void)
{
    if (g_storage_switch_busy)
    {
        return;
    }
		 g_storage_switch_busy = 1;
    if (g_storage_mode == STORAGE_MODE_USB_MSC)
    {
        osThreadFlagsSet(usbTaskHandle, USB_TASK_FLAG_STOP);
    }
    else
    {
        g_tf_log_enable = 0;
        osThreadFlagsSet(tfTaskHandle, TF_TASK_FLAG_UNMOUNT);
    }
}


void Storage_RequestUsbMscOn(void)
{
    if (g_storage_switch_busy || g_storage_mode == STORAGE_MODE_USB_MSC)
    {
        return;
    }

    g_storage_switch_busy = 1;
    g_tf_log_enable = 0;
    osThreadFlagsSet(tfTaskHandle, TF_TASK_FLAG_UNMOUNT);
}

void Storage_RequestUsbMscOff(void)
{
    if (g_storage_switch_busy || g_storage_mode == STORAGE_MODE_LOCAL_LOG)
    {
        return;
    }

    g_storage_switch_busy = 1;
    osThreadFlagsSet(usbTaskHandle, USB_TASK_FLAG_STOP);
}

static void Storage_MountLocalFs(void)
{
    if (!Storage_LockFs(osWaitForever))
    {
        printf("tfFsMutex acquire failed\r\n");
        g_tf_log_enable = 0;
        g_storage_switch_busy = 0;
        return;
    }

    if (TF_Card_MountFS())
    {
        g_tf_log_enable = 1;
        g_storage_mode = STORAGE_MODE_LOCAL_LOG;
        printf("Local TF log mode ON\r\n");
    }
    else
    {
        g_tf_log_enable = 0;
        printf("TF mount failed\r\n");
    }

    Storage_UnlockFs();
    g_storage_switch_busy = 0;
}


static void Storage_UnmountLocalFs(void)
{
    if (!Storage_LockFs(osWaitForever))
    {
        printf("tfFsMutex acquire failed\r\n");
        g_storage_switch_busy = 0;
        return;
    }

    g_tf_log_enable = 0;
    TF_Card_UnmountFS();
    Storage_UnlockFs();

    printf("TF Card FS unmounted\r\n");
}

static void Storage_StartUsbMsc(void)
{
    if (!g_usb_msc_started)
    {
        MX_USB_DEVICE_Init();
        g_usb_msc_started = 1;
    }

    g_storage_mode = STORAGE_MODE_USB_MSC;
    printf("USB MSC mode ON\r\n");
    g_storage_switch_busy = 0;
}

static void Storage_StopUsbMsc(void)
{
    if (g_usb_msc_started)
    {
        USBD_Stop(&hUsbDeviceFS);
        USBD_DeInit(&hUsbDeviceFS);
        g_usb_msc_started = 0;
    }

    printf("USB MSC mode OFF\r\n");
}

void tf_card_task(void *argument)
{
    uint32_t flags;

    (void)argument;

    if (TF_Card_Init())
    {
        TF_Card_GetInfo();

#if STORAGE_DEFAULT_USB_MSC == 0
        Storage_MountLocalFs();
#else
        g_tf_log_enable = 0;
        g_storage_mode = STORAGE_MODE_LOCAL_LOG;
        osThreadFlagsSet(usbTaskHandle, USB_TASK_FLAG_START);
#endif
    }
    else
    {
        g_tf_log_enable = 0;
        g_storage_switch_busy = 0;
        printf("TF Card init failed\r\n");
    }

    for (;;)
    {
        flags = osThreadFlagsWait(TF_TASK_FLAG_MOUNT | TF_TASK_FLAG_UNMOUNT,
                                  osFlagsWaitAny,
                                  osWaitForever);

        if (flags & TF_TASK_FLAG_UNMOUNT)
        {
            Storage_UnmountLocalFs();
            osDelay(100);
            osThreadFlagsSet(usbTaskHandle, USB_TASK_FLAG_START);
        }

        if (flags & TF_TASK_FLAG_MOUNT)
        {
            Storage_MountLocalFs();
        }
    }
}

void usb_msc_task(void *argument)
{
    uint32_t flags;

    (void)argument;

    for (;;)
    {
        flags = osThreadFlagsWait(USB_TASK_FLAG_START | USB_TASK_FLAG_STOP,
                                  osFlagsWaitAny,
                                  osWaitForever);

        if (flags & USB_TASK_FLAG_START)
        {
            Storage_StartUsbMsc();
        }

        if (flags & USB_TASK_FLAG_STOP)
        {
            Storage_StopUsbMsc();
            osDelay(100);
            osThreadFlagsSet(tfTaskHandle, TF_TASK_FLAG_MOUNT);
        }
    }
}

