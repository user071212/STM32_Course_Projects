/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "i2c.h"
#include "usart.h"
#include "ssd1306.h"
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
#define UART_RX_BUFFER_SIZE   256U
#define UART_ECHO_QUEUE_DEPTH 4U

static osMutexId_t i2cMutexHandle;
/* 保护 IMU 原始数据和姿态角，避免多个任务同时读写同一组数据。 */
static osMutexId_t attitudeMutexHandle;

/* DMA 接收缓冲区只负责接收当前这一帧。 */
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
/* 回传队列使用独立缓冲区，避免 DMA 发送时被下一帧覆盖。 */
static uint8_t uart_echo_queue[UART_ECHO_QUEUE_DEPTH][UART_RX_BUFFER_SIZE];
static uint16_t uart_echo_length[UART_ECHO_QUEUE_DEPTH];
static char uart_telemetry_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t uart_echo_head = 0U;
static volatile uint8_t uart_echo_tail = 0U;
static volatile uint8_t uart_tx_active = 0U;
static volatile uint8_t uart_rx_error = 0U;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for IMU_Task */
osThreadId_t IMU_TaskHandle;
const osThreadAttr_t IMU_Task_attributes = {
  .name = "IMU_Task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for UART_Task */
osThreadId_t UART_TaskHandle;
const osThreadAttr_t UART_Task_attributes = {
  .name = "UART_Task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for OLED_Task */
osThreadId_t OLED_TaskHandle;
const osThreadAttr_t OLED_Task_attributes = {
  .name = "OLED_Task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTask_IMU(void *argument);
void StartTask_UART(void *argument);
void StartTask_OLED(void *argument);

static void UART_EchoQueuePush(const uint8_t *data, uint16_t length);
static void UART_EchoProcess(void);
static uint8_t UART_TelemetryTryQueue(void);

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
  i2cMutexHandle = osMutexNew(NULL);
  attitudeMutexHandle = osMutexNew(NULL);
  if ((i2cMutexHandle == NULL) || (attitudeMutexHandle == NULL))
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of IMU_Task */
  IMU_TaskHandle = osThreadNew(StartTask_IMU, NULL, &IMU_Task_attributes);

  /* creation of UART_Task */
  UART_TaskHandle = osThreadNew(StartTask_UART, NULL, &UART_Task_attributes);

  /* creation of OLED_Task */
  OLED_TaskHandle = osThreadNew(StartTask_OLED, NULL, &OLED_Task_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
  if ((defaultTaskHandle == NULL) ||
      (IMU_TaskHandle == NULL) ||
      (UART_TaskHandle == NULL) ||
      (OLED_TaskHandle == NULL))
  {
    Error_Handler();
  }
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
  (void)argument;
  for (;;)
  {
    osDelay(1000U);
  }
}

/* USER CODE BEGIN Header_StartTask_IMU */
/**
* @brief Function implementing the IMU_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_IMU */
void StartTask_IMU(void *argument)
{
  uint32_t previousTick;

  (void)argument;
  previousTick = HAL_GetTick();

  for (;;)
  {
    uint8_t sensorReady = 0U;
    if (osMutexAcquire(attitudeMutexHandle, osWaitForever) == osOK)
    {
      sensorReady = mpu6050_ok;
      (void)osMutexRelease(attitudeMutexHandle);
    }

    if (sensorReady != 0U)
    {
      HAL_StatusTypeDef readStatus = HAL_ERROR;
      int16_t rawAccelX = 0;
      int16_t rawAccelY = 0;
      int16_t rawAccelZ = 0;
      int16_t rawGyroZ = 0;

      if (osMutexAcquire(i2cMutexHandle, osWaitForever) == osOK)
      {
        /* 读传感器时也锁住数据，防止串口任务读到一半更新的数据。 */
        if (osMutexAcquire(attitudeMutexHandle, osWaitForever) == osOK)
        {
          readStatus = MPU6050_ReadRaw();
          if (readStatus == HAL_OK)
          {
            rawAccelX = accel_x;
            rawAccelY = accel_y;
            rawAccelZ = accel_z;
            rawGyroZ = gyro_z;
          }
          (void)osMutexRelease(attitudeMutexHandle);
        }
        (void)osMutexRelease(i2cMutexHandle);
      }

      if (readStatus == HAL_OK)
      {
        /* MPU6050 当前量程对应：加速度 16384 LSB/g，陀螺仪 131 LSB/(°/s)。 */
        float ax_g = (float)rawAccelX / 16384.0f;
        float ay_g = (float)rawAccelY / 16384.0f;
        float az_g = (float)rawAccelZ / 16384.0f;
        float gz_dps = (float)rawGyroZ / 131.0f;
        uint32_t now = HAL_GetTick();
        uint32_t elapsed = now - previousTick;
        float newRoll;
        float newPitch;
        float newYaw;

        previousTick = now;
        /* 静止或低速转动时，重力方向可以给出 Roll/Pitch。 */
        newRoll = atan2f(ay_g, az_g) * 57.29578f;
        newPitch = atan2f(-ax_g, sqrtf((ay_g * ay_g) + (az_g * az_g))) * 57.29578f;

        if (osMutexAcquire(attitudeMutexHandle, osWaitForever) == osOK)
        {
          /* Yaw 没有磁力计参考，这里用 Z 轴角速度积分，并限制到 ±180°。 */
          newYaw = yaw_gyro + gz_dps * ((float)elapsed * 0.001f);
          if (newYaw > 180.0f)
          {
            newYaw -= 360.0f;
          }
          else if (newYaw < -180.0f)
          {
            newYaw += 360.0f;
          }
          roll_acc = newRoll;
          pitch_acc = newPitch;
          yaw_gyro = newYaw;
          (void)osMutexRelease(attitudeMutexHandle);
        }
      }
      else
      {
        if (osMutexAcquire(attitudeMutexHandle, osWaitForever) == osOK)
        {
          mpu6050_ok = 0U;
          (void)osMutexRelease(attitudeMutexHandle);
        }
      }
    }

    osDelay(10U);
  }
}

/* USER CODE BEGIN Header_StartTask_UART */
/**
* @brief Function implementing the UART_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_UART */
void StartTask_UART(void *argument)
{
  uint32_t lastTelemetryTick;

  (void)argument;

  /* 任务启动后开启 DMA 不定长接收，避免在 RTOS 尚未运行时触发回调。 */
  if (HAL_UARTEx_ReceiveToIdle_DMA(
          &huart1,
          uart_rx_buffer,
          UART_RX_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
  /* 关闭半传输通知，只在一帧真正结束（检测到 IDLE）时处理。 */
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  lastTelemetryTick = HAL_GetTick();

  for (;;)
  {
    /* 收到的数据在队列中按原长度原样发回。 */
    UART_EchoProcess();

    /* 队列空闲时，定时把 IMU 原始数据和姿态角发给串口助手。 */
    if ((HAL_GetTick() - lastTelemetryTick) >= 500U)
    {
      if (UART_TelemetryTryQueue() != 0U)
      {
        lastTelemetryTick = HAL_GetTick();
      }
    }

    if (uart_rx_error != 0U)
    {
      /* 重新启动接收，给串口故障留下恢复机会。 */
      uart_rx_error = 0U;
      if (HAL_UARTEx_ReceiveToIdle_DMA(
              &huart1,
              uart_rx_buffer,
              UART_RX_BUFFER_SIZE) != HAL_OK)
      {
        uart_rx_error = 1U;
      }
      else
      {
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
      }
    }

    osDelay(1U);
  }
}

/* USER CODE BEGIN Header_StartTask_OLED */
/**
* @brief Function implementing the OLED_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_OLED */
void StartTask_OLED(void *argument)
{
  char line[24];

  (void)argument;
  for (;;)
  {
    uint8_t sensorReady = 0U;
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;

    if (osMutexAcquire(attitudeMutexHandle, osWaitForever) == osOK)
    {
      sensorReady = mpu6050_ok;
      roll = roll_acc;
      pitch = pitch_acc;
      yaw = yaw_gyro;
      (void)osMutexRelease(attitudeMutexHandle);
    }

    ssd1306_Fill(Black);
    ssd1306_SetCursor(0U, 0U);
    ssd1306_WriteString("IMU Attitude", Font_7x10, White);

    if (sensorReady != 0U)
    {
      ssd1306_SetCursor(0U, 16U);
      (void)snprintf(line, sizeof(line), "Roll: %.2f", (double)roll);
      ssd1306_WriteString(line, Font_7x10, White);
      ssd1306_WriteDegree(White);

      ssd1306_SetCursor(0U, 32U);
      (void)snprintf(line, sizeof(line), "Pitch: %.2f", (double)pitch);
      ssd1306_WriteString(line, Font_7x10, White);
      ssd1306_WriteDegree(White);

      ssd1306_SetCursor(0U, 48U);
      (void)snprintf(line, sizeof(line), "Yaw: %.2f", (double)yaw);
      ssd1306_WriteString(line, Font_7x10, White);
      ssd1306_WriteDegree(White);
    }
    else
    {
      ssd1306_SetCursor(0U, 16U);
      ssd1306_WriteString("Sensor Error", Font_7x10, White);
    }

    if (osMutexAcquire(i2cMutexHandle, osWaitForever) == osOK)
    {
      ssd1306_UpdateScreen(&hi2c1);
      (void)osMutexRelease(i2cMutexHandle);
    }

    osDelay(100U);
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

static void UART_EchoQueuePush(const uint8_t *data, uint16_t length)
{
  uint8_t nextHead;
  uint32_t previousPrimask;

  if ((length == 0U) || (length > UART_RX_BUFFER_SIZE))
  {
    return;
  }

  /* 接收回调运行在中断中，发送任务也可能写入同一个队列。 */
  previousPrimask = __get_PRIMASK();
  __disable_irq();

  nextHead = (uint8_t)(uart_echo_head + 1U);
  if (nextHead >= UART_ECHO_QUEUE_DEPTH)
  {
    nextHead = 0U;
  }

  /* 队列已满时丢弃当前帧，保护已经排队等待发送的数据。 */
  if (nextHead == uart_echo_tail)
  {
    if (previousPrimask == 0U)
    {
      __enable_irq();
    }
    return;
  }

  (void)memcpy(uart_echo_queue[uart_echo_head], data, length);
  uart_echo_length[uart_echo_head] = length;
  uart_echo_head = nextHead;

  if (previousPrimask == 0U)
  {
    __enable_irq();
  }
}

/**
 * @brief 把当前 IMU 数据排成一段易读的文字，放入串口发送队列。
 * @retval 1 成功排队，0 队列当前忙或数据无效。
 */
static uint8_t UART_TelemetryTryQueue(void)
{
  uint8_t sensorReady = 0U;
  float roll = 0.0f;
  float pitch = 0.0f;
  float yaw = 0.0f;
  int messageLength;
  uint16_t txLength;
  uint8_t pushed = 0U;
  uint32_t previousPrimask;

  if (osMutexAcquire(attitudeMutexHandle, osWaitForever) == osOK)
  {
    sensorReady = mpu6050_ok;
    roll = roll_acc;
    pitch = pitch_acc;
    yaw = yaw_gyro;
    (void)osMutexRelease(attitudeMutexHandle);
  }

  if (sensorReady == 0U)
  {
    messageLength = snprintf(
        uart_telemetry_buffer,
        sizeof(uart_telemetry_buffer),
        "MPU6050 ERROR\r\n");
  }
  else
  {
    messageLength = snprintf(
        uart_telemetry_buffer,
        sizeof(uart_telemetry_buffer),
        "IMU Attitude\r\n"
        "Roll: %.2f deg\r\n"
        "Pitch: %.2f deg\r\n"
        "Yaw: %.2f deg\r\n",
        (double)roll,
        (double)pitch,
        (double)yaw);
  }

  if (messageLength <= 0)
  {
    return 0U;
  }

  txLength = (messageLength >= (int)sizeof(uart_telemetry_buffer))
                 ? (uint16_t)(sizeof(uart_telemetry_buffer) - 1U)
                 : (uint16_t)messageLength;

  /* 只有发送队列完全空闲时才加入遥测，保证收到的回传优先。 */
  previousPrimask = __get_PRIMASK();
  __disable_irq();
  if ((uart_tx_active == 0U) && (uart_echo_tail == uart_echo_head))
  {
    UART_EchoQueuePush((const uint8_t *)uart_telemetry_buffer, txLength);
    pushed = 1U;
  }
  if (previousPrimask == 0U)
  {
    __enable_irq();
  }

  return pushed;
}

static void UART_EchoProcess(void)
{
  uint8_t tail;

  if ((uart_tx_active != 0U) || (uart_echo_tail == uart_echo_head))
  {
    return;
  }

  tail = uart_echo_tail;
  uart_tx_active = 1U;
  if (HAL_UART_Transmit_DMA(
          &huart1,
          uart_echo_queue[tail],
          uart_echo_length[tail]) != HAL_OK)
  {
    /* DMA 忙时不删除队头，下一次循环继续尝试。 */
    uart_tx_active = 0U;
  }
}

/* HAL 检测到串口空闲时调用，Size 就是这一帧收到的实际长度。 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  UART_EchoQueuePush(uart_rx_buffer, Size);

  /* 重新开启 DMA，下一帧可以是不同长度。 */
  if (HAL_UARTEx_ReceiveToIdle_DMA(
          &huart1,
          uart_rx_buffer,
          UART_RX_BUFFER_SIZE) != HAL_OK)
  {
    uart_rx_error = 1U;
  }
  else
  {
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  }
}

/* 一帧 DMA 发送完成后，释放队列当前位置。 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t nextTail;

  if (huart->Instance != USART1)
  {
    return;
  }

  nextTail = (uint8_t)(uart_echo_tail + 1U);
  if (nextTail >= UART_ECHO_QUEUE_DEPTH)
  {
    nextTail = 0U;
  }
  uart_echo_tail = nextTail;
  uart_tx_active = 0U;
}

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
