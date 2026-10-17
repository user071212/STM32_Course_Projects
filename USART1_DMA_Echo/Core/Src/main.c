/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

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

/* USER CODE BEGIN PV */
#define UART_RX_BUFFER_SIZE   256U
#define UART_ECHO_QUEUE_DEPTH 4U

/* 用来保存dma接收到的数据，当作缓冲区 */
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];

/* 回传队列使用独立缓冲区，避免dma发送时被下一帧接收数据覆盖 */
static uint8_t uart_echo_queue[UART_ECHO_QUEUE_DEPTH][UART_RX_BUFFER_SIZE];
static uint16_t uart_echo_length[UART_ECHO_QUEUE_DEPTH];
static volatile uint8_t uart_echo_head = 0U;
static volatile uint8_t uart_echo_tail = 0U;
static volatile uint8_t uart_tx_active = 0U;
static volatile uint8_t uart_rx_error = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void UART_EchoQueuePush(const uint8_t *data, uint16_t length);
static void UART_EchoProcess(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 启动DMA不定长接收。串口线保持空闲时，HAL会通知一帧已经结束 */
  if (HAL_UARTEx_ReceiveToIdle_DMA(
          &huart1,
          uart_rx_buffer,
          UART_RX_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }

  /* 只关掉 DMA 半传输通知，避免一帧数据还没收完就提前回传。 */
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    UART_EchoProcess();
    if (uart_rx_error != 0U)
    {
      Error_Handler();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * @brief 把一帧接收到的数据复制到发送队列。
 * @param data 指向待发送数据的缓冲区。
 * @param length 数据字节数，最大为 UART_RX_BUFFER_SIZE。
 *
 * 接收 DMA 会很快开始接收下一帧，所以不能直接发送 uart_rx_buffer；
 * 这里先复制到独立队列，保证回传内容不会被下一帧覆盖。
 */
static void UART_EchoQueuePush(const uint8_t *data, uint16_t length)
{
  uint8_t next_head;

  if (length == 0U || length > UART_RX_BUFFER_SIZE)
  {
    return;
  }

  next_head = (uint8_t)(uart_echo_head + 1U);
  if (next_head >= UART_ECHO_QUEUE_DEPTH)
  {
    next_head = 0U;
  }

  /* 队列已满时丢弃当前帧，防止覆盖还没有发送的数据。 */
  if (next_head == uart_echo_tail)
  {
    return;
  }

  (void)memcpy(
      uart_echo_queue[uart_echo_head],
      data,
      length);
  uart_echo_length[uart_echo_head] = length;
  uart_echo_head = next_head;
}

/**
 * @brief 启动队首数据的 DMA 发送。
 *
 * 每次只启动一帧，发送完成后由 HAL_UART_TxCpltCallback() 移动队列队尾。
 */
static void UART_EchoProcess(void)
{
  uint8_t tail;

  if (uart_tx_active != 0U || uart_echo_tail == uart_echo_head)
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
    /* DMA 忙时下次循环再试 */
    uart_tx_active = 0U;
  }
}

/**
 * @brief 处理 UART DMA 的不定长接收完成事件。
 * @param huart 触发事件的串口句柄。
 * @param Size 本帧实际收到的字节数。
 *
 * HAL 检测到串口线空闲（IDLE）后调用此函数，因此 Size 不需要预先固定。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  UART_EchoQueuePush(uart_rx_buffer, Size);

  /* 重新打开 DMA，等待下一帧，下一帧可以是任意长度 */
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

/**
 * @brief 处理一帧 DMA 发送完成事件。
 * @param huart 触发事件的串口句柄。
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t next_tail;

  if (huart->Instance != USART1)
  {
    return;
  }

  next_tail = (uint8_t)(uart_echo_tail + 1U);
  if (next_tail >= UART_ECHO_QUEUE_DEPTH)
  {
    next_tail = 0U;
  }
  uart_echo_tail = next_tail;
  uart_tx_active = 0U;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
