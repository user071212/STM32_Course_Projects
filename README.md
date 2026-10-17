# STM32 串口 DMA、MPU6050 与 OLED 综合实验

本仓库包含两个 STM32F103C8T6 实验工程，使用 STM32CubeMX 生成初始化代码，使用 Keil MDK5 编译下载。代码保留了 CubeMX 工程、Keil 工程、HAL 驱动、FreeRTOS 源码和 `.ioc` 配置文件，克隆后可以直接打开对应的 Keil 工程。

## 项目简介

### 2.1 USART1_DMA_Echo

使用 USART1 和 DMA 实现不定长串口接收。串口线出现空闲（IDLE）时，HAL 回调给出本帧实际长度，程序把本帧复制到发送队列，再使用 DMA 原样发送回电脑。每一帧长度可以不同，回传内容与接收内容逐字节一致。

### 2.2 / 2.3 / 2.4 STM32_IMU_OLED_FreeRTOS

使用 I2C1 连接 MPU6050 和 SSD1306 OLED，读取 MPU6050 的加速度和陀螺仪数据，计算 Roll、Pitch、Yaw，并把姿态角显示在 OLED 上。工程使用 FreeRTOS 划分 IMU、串口和 OLED 任务；串口仍保留 2.1 的 DMA 不定长原样回传，同时定时输出当前姿态角。

## 硬件清单与芯片型号

| 器件 | 型号或规格 | 用途 |
|---|---|---|
| STM32 开发板 | STM32F103C8T6（Blue Pill 类开发板） | 主控 |
| IMU | MPU6050 / GY-521，I2C 地址 0x68 | 三轴加速度和三轴陀螺仪 |
| OLED | SSD1306，128×64，I2C 地址 0x3C | 显示姿态角 |
| USB 转 TTL | 3.3 V 电平 | 电脑串口助手通信 |
| 下载器 | ST-Link | Keil 下载和调试 |

所有信号建议使用 3.3 V 电平。MPU6050 和 OLED 的 I2C 总线可以并联，但两个模块的地址必须不同。

## 引脚接线表

### 2.1 串口 DMA 工程

| STM32 引脚 | 连接对象 | 说明 |
|---|---|---|
| PA9 | USB-TTL RXD | USART1_TX，STM32 发给电脑 |
| PA10 | USB-TTL TXD | USART1_RX，电脑发给 STM32 |
| GND | USB-TTL GND | 必须共地 |
| 3.3V | USB-TTL VCC（可选） | 只在电源需求明确时连接 |

### 综合工程

| STM32 引脚 | 连接对象 | 说明 |
|---|---|---|
| PB6 | MPU6050 SCL、SSD1306 SCL | I2C1_SCL |
| PB7 | MPU6050 SDA、SSD1306 SDA | I2C1_SDA |
| PA9 | USB-TTL RXD | USART1_TX |
| PA10 | USB-TTL TXD | USART1_RX |
| 3.3V | MPU6050、OLED VCC | 使用模块允许的电压 |
| GND | MPU6050、OLED、USB-TTL GND | 所有设备共地 |

## CubeMX 配置说明

两个工程都使用 STM32F103C(8-B)Tx，外部高速晶振通过 PLL 得到 72 MHz 系统时钟。

### 2.1 配置

- USART1：异步模式，115200 bit/s，8 数据位，无校验，1 停止位。
- PA9 配置为 USART1_TX，PA10 配置为 USART1_RX。
- DMA1 Channel 5：USART1_RX，外设到内存，Normal 模式。
- DMA1 Channel 4：USART1_TX，内存到外设，Normal 模式。
- 打开 USART1 和两个 DMA 通道中断。
- 代码中调用 `HAL_UARTEx_ReceiveToIdle_DMA()`，并关闭 DMA 半传输中断，只在一帧结束时处理。

### 综合工程配置

- 保留上面的 USART1 和 DMA 配置。
- I2C1：PB6/PB7，7 位地址，100 kHz。
- FreeRTOS 使用 CMSIS-RTOS2 接口，堆大小 8192 字节。
- 时基使用 TIM1，避免 FreeRTOS 与 HAL 共用同一个 SysTick。
- 生成的 `.ioc` 文件为 `STM32_IMU_OLED_FreeRTOS/STM32_IMU_OLED_FreeRTOS.ioc`。

## FreeRTOS 任务结构

综合工程中有以下任务：

| 任务 | 优先级 | 周期 | 作用 |
|---|---:|---:|---|
| `IMU_Task` | High | 10 ms | 读取 MPU6050，并计算姿态角 |
| `OLED_Task` | Normal | 100 ms | 刷新 OLED 上的四行文字 |
| `UART_Task` | Low | 1 ms 调度 | DMA 不定长回传；每 500 ms 输出姿态角 |
| `defaultTask` | Normal | 1000 ms | 保留的空闲示例任务 |

I2C 总线使用 `i2cMutexHandle` 保护，姿态数据使用 `attitudeMutexHandle` 保护。这样 IMU 任务不会和 OLED 任务同时访问 I2C，串口任务也不会读到更新了一半的数据。

## 代码目录说明

```text
STM32_Course_Projects/
├─ USART1_DMA_Echo/
│  ├─ Core/                 应用代码、串口回调、HAL 初始化
│  ├─ Drivers/              STM32 HAL 和 CMSIS 必需文件
│  ├─ MDK-ARM/              Keil5 工程文件和启动文件
│  └─ USART1_DMA_Echo.ioc   CubeMX 配置
├─ STM32_IMU_OLED_FreeRTOS/
│  ├─ Core/                 MPU6050、FreeRTOS、OLED 和外设代码
│  ├─ Drivers/              STM32 HAL 和 CMSIS 必需文件
│  ├─ Middlewares/          FreeRTOS 源码
│  ├─ MDK-ARM/              Keil5 工程文件和启动文件
│  └─ STM32_IMU_OLED_FreeRTOS.ioc
├─ .gitignore
└─ README.md
```

`MDK-ARM` 下的 `uvprojx` 是 Keil 工程入口；`uvoptx` 是 Keil 的调试选项文件。编译产生的 `axf、hex、map、dep、htm` 等文件被 `.gitignore` 排除。

## 串口通信收发处理流程

### 2.1 回传流程

1. UART 任务调用 `HAL_UARTEx_ReceiveToIdle_DMA()` 开始接收。
2. 电脑发送一帧任意长度数据，串口线空闲后触发 `HAL_UARTEx_RxEventCallback()`。
3. 回调使用 `Size` 得到本帧实际字节数，把数据复制到环形发送队列。
4. UART 任务调用 `HAL_UART_Transmit_DMA()` 发送队首数据。
5. 发送完成回调移动队列尾部，等待下一帧。
6. RX DMA 每次回调后重新启动，所以每一帧长度可以不同。

### 综合工程附加输出

综合工程仍然按照上面的流程原样回传电脑发来的数据。UART 任务在发送队列空闲时，每 500 ms 把当前 `Roll、Pitch、Yaw` 加入发送队列，串口助手可以看到姿态角。电脑发送的数据优先于周期状态输出。

## IMU 型号、接口、校准与姿态解算

- 型号：MPU6050。
- 接口：I2C1，PB6 为 SCL，PB7 为 SDA，7 位地址 0x68。
- 上电检查：读取 `WHO_AM_I`，期望值为 0x68。
- 基础配置：唤醒芯片；加速度量程设为 ±2g；陀螺仪量程设为 ±250°/s；采样分频寄存器设为 7。
- 原始数据：从 `ACCEL_XOUT_H` 连续读取 14 字节，解析三轴加速度、温度和三轴陀螺仪。
- 换算：加速度使用 16384 LSB/g，陀螺仪使用 131 LSB/(°/s)。
- Roll/Pitch：使用加速度重力方向，通过 `atan2f` 计算。
- Yaw：使用 Z 轴角速度按采样时间积分，并限制在 -180° 到 180°。

当前示例完成了量程和寄存器配置，没有加入上电静止零偏自动标定，也没有磁力计。因此 Yaw 长时间运行会有漂移；需要更高精度时，可在开机静止阶段采集陀螺仪零偏，或增加磁力计做航向修正。

## OLED 显示说明

SSD1306 使用 I2C 地址 0x3C，分辨率 128×64。OLED 任务每 100 ms 清空并刷新一次屏幕，显示：

```text
IMU Attitude
Roll: 12.34°
Pitch: -5.67°
Yaw: 89.01°
```

代码额外绘制了一个简单的度数符号，避免字体表中没有 `°` 导致显示乱码。传感器通信失败时显示 `Sensor Error`。

## 编译与烧录步骤

1. 安装 Keil MDK5、STM32F1xx Device Pack 和 STM32CubeMX。
2. 打开对应的 `.uvprojx`：
   - `USART1_DMA_Echo/MDK-ARM/USART1_DMA_Echo.uvprojx`
   - `STM32_IMU_OLED_FreeRTOS/MDK-ARM/STM32_IMU_OLED_FreeRTOS.uvprojx`
3. 在 Keil 中选择 `Rebuild`，确认无 error、无 warning。
4. 连接 ST-Link，选择 `Download` 烧录。
5. 综合工程连接 OLED、MPU6050 和 USB-TTL 后运行；串口助手设置为 115200、8 数据位、无校验、1 停止位。
6. 发送任意长度字符串，确认 STM32 原样回传；综合工程等待约 500 ms，确认串口显示姿态角，OLED 显示四行姿态信息。

## 遇到的问题与解决方法

| 问题 | 解决方法 |
|---|---|
| 不定长 DMA 接收提前触发 | RX DMA 使用 Normal 模式，并关闭半传输中断，只处理 IDLE 回调 |
| DMA 接收缓冲区被下一帧覆盖 | 接收回调先把数据复制到独立的发送队列 |
| 回传数据偶尔丢失或顺序异常 | 发送队列使用队头、队尾和 DMA 忙标志，发送完成后再释放队列项 |
| FreeRTOS 任务同时访问 I2C | IMU 和 OLED 共用 I2C 互斥锁 |
| OLED 上的角度符号显示异常 | 使用 `ssd1306_WriteDegree()` 绘制度数符号 |
| MPU6050 能读数据但 Yaw 慢慢变化 | 这是陀螺仪积分漂移，需做零偏标定或增加磁力计 |
| 仓库体积过大 | 只保留 Keil 工程实际需要的 HAL/CMSIS/FreeRTOS 文件，并通过 `.gitignore` 排除编译中间文件 |

## 许可证与第三方代码

STM32 HAL、CMSIS、FreeRTOS 和 SSD1306 驱动中的原始版权说明保留在对应源文件或目录中。应用层代码用于课程实验和学习。