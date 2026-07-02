#include "shared_bus_lock.h"
#include "cmsis_os2.h"
#include "main.h"
#include "FreeRTOS.h"

static osMutexId_t g_sharedBusMutex = NULL;
static const osMutexAttr_t g_sharedBusMutexAttr = {
    .name = "sharedBusMutex"
};

static void SharedBus_EnsureMutex(void)
{
    osKernelState_t state = osKernelGetState();

    if (g_sharedBusMutex != NULL)
        return;

    if ((state == osKernelReady) ||
        (state == osKernelRunning) ||
        (state == osKernelLocked) ||
        (state == osKernelSuspended))
    {
        g_sharedBusMutex = osMutexNew(&g_sharedBusMutexAttr);
        configASSERT(g_sharedBusMutex != NULL);
    }
}

uint8_t SharedBus_Lock(uint32_t timeout)
{
    SharedBus_EnsureMutex();

    /* 内核还没起来时，默认单线程初始化，直接放行 */
    if (g_sharedBusMutex == NULL)
        return 1U;

    return (osMutexAcquire(g_sharedBusMutex, timeout) == osOK) ? 1U : 0U;
}

void SharedBus_Unlock(void)
{
    if (g_sharedBusMutex != NULL)
    {
        osMutexRelease(g_sharedBusMutex);
    }
}

void SharedBus_PrepareForSPL06(void)
{
    /* 先保证 NRF24 片选无效，避免它误收时钟 */
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_7, GPIO_PIN_SET);

    /* SPL06 空闲态 */
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_SET);
}

void SharedBus_PrepareForNRF24(void)
{
    /* 先保证 SPL06 片选无效，避免它误收时钟 */
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_SET);

    /* NRF24 空闲态 */
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_RESET);
}

