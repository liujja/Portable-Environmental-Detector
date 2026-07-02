/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sys_task.h"
#include "storage_manager.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

#if ENABLE_BSP_BUZZER
osThreadId_t buzzerTaskHandle;
const osThreadAttr_t buzzerTask_attributes = {
  .name = "buzzerTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
#endif

#if ENABLE_BSP_KEY
osThreadId_t keyTaskHandle;
const osThreadAttr_t keyTask_attributes = {
  .name = "keyTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
#endif

#if ENABLE_BSP_MLX90640
osThreadId_t mlx90640TaskHandle;
const osThreadAttr_t mlx90640Task_attributes = {
  .name = "mlx90640Task",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
#endif

osThreadId_t tfTaskHandle;
const osThreadAttr_t tfTask_attributes = {
  .name = "tfTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

osThreadId_t usbTaskHandle;
const osThreadAttr_t usbTask_attributes = {
  .name = "usbTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

osMutexId_t tfFsMutexHandle;
const osMutexAttr_t tfFsMutex_attributes = {
  .name = "tfFsMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
extern void display_task(void *argument);
extern void sensor_task(void *argument);
extern void tf_card_task(void *argument);
extern void usb_msc_task(void *argument);
#if ENABLE_BSP_BUZZER
extern void buzzer_task(void *argument);
#endif
#if ENABLE_BSP_KEY
extern void key_task(void *argument);
#endif
#if ENABLE_BSP_MLX90640
extern void mlx90640_task(void *argument);
#endif

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);


void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
	tfFsMutexHandle = osMutexNew(&tfFsMutex_attributes);
  configASSERT(tfFsMutexHandle != NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  configASSERT(defaultTaskHandle != NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  displayTaskHandle = osThreadNew(display_task, NULL, &displayTask_attributes);
  configASSERT(displayTaskHandle != NULL);

  sensorTaskHandle = osThreadNew(sensor_task, NULL, &sensorTask_attributes);
  configASSERT(sensorTaskHandle != NULL);

#if ENABLE_BSP_BUZZER
  buzzerTaskHandle = osThreadNew(buzzer_task, NULL, &buzzerTask_attributes);
  configASSERT(buzzerTaskHandle != NULL);
#endif
	
	
	tfTaskHandle = osThreadNew(tf_card_task, NULL, &tfTask_attributes);
	configASSERT(tfTaskHandle != NULL);

	usbTaskHandle = osThreadNew(usb_msc_task, NULL, &usbTask_attributes);
	configASSERT(usbTaskHandle != NULL);

#if ENABLE_BSP_KEY
  keyTaskHandle = osThreadNew(key_task, NULL, &keyTask_attributes);
  configASSERT(keyTaskHandle != NULL);
#endif

#if ENABLE_BSP_MLX90640
  mlx90640TaskHandle = osThreadNew(mlx90640_task, NULL, &mlx90640Task_attributes);
  configASSERT(mlx90640TaskHandle != NULL);
#endif
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

