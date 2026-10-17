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
#include "cmsis_os.h"
#include "dma.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MPU6050_ADDR             (0x68U << 1)
#define MPU6050_REG_WHO_AM_I     0x75U
#define MPU6050_REG_PWR_MGMT_1   0x6BU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile int16_t accel_x = 0;
volatile int16_t accel_y = 0;
volatile int16_t accel_z = 0;

volatile int16_t gyro_x = 0;
volatile int16_t gyro_y = 0;
volatile int16_t gyro_z = 0;

volatile uint8_t mpu6050_ok = 0U;

volatile float roll_acc = 0.0f;
volatile float pitch_acc = 0.0f;
volatile float yaw_gyro = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/** @brief 向 MPU6050 的一个寄存器写入一个字节。 */
static HAL_StatusTypeDef MPU6050_WriteReg(
    uint8_t reg,
    uint8_t data)
{
    return HAL_I2C_Mem_Write(
        &hi2c1,
        MPU6050_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        &data,
        1,
        100
    );
}

/** @brief 从 MPU6050 连续读取多个寄存器。 */
static HAL_StatusTypeDef MPU6050_ReadRegs(
    uint8_t reg,
    uint8_t *data,
    uint16_t len)
{
    return HAL_I2C_Mem_Read(
        &hi2c1,
        MPU6050_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        data,
        len,
        100
    );
}

/**
 * @brief 检查 MPU6050 身份并完成基础配置。
 *
 * 配置内容包括唤醒芯片、加速度量程 ±2g、陀螺仪量程 ±250°/s，
 * 并把采样分频设置为较容易观察的速度。
 */
HAL_StatusTypeDef MPU6050_Init(void)
{
    uint8_t id = 0;

    if (MPU6050_ReadRegs(
            MPU6050_REG_WHO_AM_I, &id, 1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (id != 0x68)
    {
        return HAL_ERROR;
    }

    if (MPU6050_WriteReg(
            MPU6050_REG_PWR_MGMT_1, 0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(100);

    if (MPU6050_WriteReg(0x1B, 0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (MPU6050_WriteReg(0x1C, 0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (MPU6050_WriteReg(0x19, 0x07) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief 读取一组完整的加速度和陀螺仪原始数据。
 *
 * 从 ACCEL_XOUT_H 开始连续读取 14 字节，跳过温度数据后解析出
 * 三轴加速度和三轴陀螺仪数据。
 */
HAL_StatusTypeDef MPU6050_ReadRaw(void)
{
    uint8_t buf[14];

    if (MPU6050_ReadRegs(
            MPU6050_REG_ACCEL_XOUT_H,
            buf,
            14) != HAL_OK)
    {
        return HAL_ERROR;
    }

    accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    accel_z = (int16_t)((buf[4] << 8) | buf[5]);

    gyro_x = (int16_t)((buf[8] << 8) | buf[9]);
    gyro_y = (int16_t)((buf[10] << 8) | buf[11]);
    gyro_z = (int16_t)((buf[12] << 8) | buf[13]);

    return HAL_OK;
}
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
  MX_I2C1_Init();
    /* USER CODE BEGIN 2 */
  /* Initialize sensors and the display before starting the scheduler. */
  mpu6050_ok = (MPU6050_Init() == HAL_OK) ? 1U : 0U;
  (void)ssd1306_Init(&hi2c1);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in freertos.c) */
  MX_FREERTOS_Init();
  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1){
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
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
/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
