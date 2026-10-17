/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>

/** @brief MPU6050 三轴加速度原始计数值。 */
extern volatile int16_t accel_x;
extern volatile int16_t accel_y;
extern volatile int16_t accel_z;

/** @brief MPU6050 三轴陀螺仪原始计数值。 */
extern volatile int16_t gyro_x;
extern volatile int16_t gyro_y;
extern volatile int16_t gyro_z;

/** @brief 当前姿态角，单位为度。 */
extern volatile float roll_acc;
extern volatile float pitch_acc;
extern volatile float yaw_gyro;

/** @brief MPU6050 是否已成功初始化并可正常读取。 */
extern volatile uint8_t mpu6050_ok;



/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/** @brief 唤醒并配置 MPU6050。 */
HAL_StatusTypeDef MPU6050_Init(void);

/** @brief 读取 MPU6050 的加速度和陀螺仪原始数据。 */
HAL_StatusTypeDef MPU6050_ReadRaw(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
