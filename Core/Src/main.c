/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32U575VGT6 串口透传（双中断 + 环形缓冲区）
  *                   USART1 ↔ 电脑（115200）
  *                   USART2 ↔ DO5151 晶振（115200）
  * @note           : 解决轮询模式下 115200 丢字节的问题
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* -------- 环形缓冲区定义 -------- */
#define RING_SIZE  256                   /* 必须是 2 的幂 */

typedef struct {
    volatile uint8_t  buf[RING_SIZE];
    volatile uint16_t head;              /* 写入位置（中断写） */
    volatile uint16_t tail;              /* 读取位置（主循环读） */
} RingBuf_t;

static RingBuf_t  rb_u1;                /* USART1 接收缓冲（电脑→单片机） */
static RingBuf_t  rb_u2;                /* USART2 接收缓冲（晶振→单片机） */

static uint8_t u1_rx_byte;              /* USART1 单字节中断接收 */
static uint8_t u2_rx_byte;              /* USART2 单字节中断接收 */

/* -------- 环形缓冲区操作 -------- */
static inline void ring_put(RingBuf_t *rb, uint8_t c)
{
    uint16_t next = (rb->head + 1) & (RING_SIZE - 1);
    if (next != rb->tail)                /* 满了就丢弃（正常不会满） */
    {
        rb->buf[rb->head] = c;
        rb->head = next;
    }
}

static inline int ring_get(RingBuf_t *rb, uint8_t *c)
{
    if (rb->head == rb->tail) return 0;  /* 空 */
    *c = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & (RING_SIZE - 1);
    return 1;
}

/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN 0 */
__asm(".global __use_no_semihosting");
void _sys_exit(int x) { (void)x; }

static void U1_Send(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}
/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */
    HAL_Delay(200);

    /* 初始化环形缓冲区 */
    rb_u1.head = rb_u1.tail = 0;
    rb_u2.head = rb_u2.tail = 0;

    /* 两个串口都用中断接收 */
    HAL_UART_Receive_IT(&huart1, &u1_rx_byte, 1);
    HAL_UART_Receive_IT(&huart2, &u2_rx_byte, 1);

    U1_Send("=== STM32 DO5151 透传就绪 ===\r\n");
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
        uint8_t c;

        /* ===== 方向1：电脑 → 晶振 ===== */
        while (ring_get(&rb_u1, &c))
        {
            HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
        }

        /* ===== 方向2：晶振 → 电脑 ===== */
        while (ring_get(&rb_u2, &c))
        {
            HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
        }

        /* USER CODE END 3 */
    }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        ring_put(&rb_u1, u1_rx_byte);
        HAL_UART_Receive_IT(&huart1, &u1_rx_byte, 1);
    }
    else if (huart->Instance == USART2)
    {
        ring_put(&rb_u2, u2_rx_byte);
        HAL_UART_Receive_IT(&huart2, &u2_rx_byte, 1);
    }
}
/* USER CODE END 4 */

/* ================================================================
 *  以下是 CubeMX 生成部分，不要修改
 * ================================================================ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE4) != HAL_OK)
        Error_Handler();

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_4;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                     | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
        Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)                                             { Error_Handler(); }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)                               { Error_Handler(); }
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance                    = USART2;
    huart2.Init.BaudRate               = 115200;
    huart2.Init.WordLength             = UART_WORDLENGTH_8B;
    huart2.Init.StopBits               = UART_STOPBITS_1;
    huart2.Init.Parity                 = UART_PARITY_NONE;
    huart2.Init.Mode                   = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)                                             { Error_Handler(); }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) { Error_Handler(); }
    if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)                               { Error_Handler(); }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif