/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#include "main.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "app_comm.h"
#include "app_sensor.h"
#include "gwmz.h"
#include "gestures_example.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Classes */
const char *classes_table[NB_CLASSES] = { "None", "FlatHand", "Like", "Love", "Dislike", "BreakTime", "CrossHands", "Fist" };

/* Application context */
AppConfig_TypeDef App_Config;

GW_proc_t gest_predictor;
HT_proc_t hand_tracker;
SEN_data_t sensor_data;

SensorTxData_t SensorTxData;
uint8_t UartTxBuffer[11] = {0,};

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define GESTURE_DEBOUNCE_MS 500
#define MIN_POSTURE_CONFIDENCE 50
/* USER CODE END PD */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
static uint32_t lastGestureTime = 0; // Tracks the last time any gesture/posture action was triggered

// --- Menu System Variables ---
// Enum for different menu states


// Global variable to track the current menu state

// Pointers to the current menu items and their count, allowing dynamic switching


// Define menu item arrays for different states
 // Current selected item index within the active menu

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART6_UART_Init(void);
GW_label_t getGestureOutput(void);
/* USER CODE BEGIN PFP */

static void Software_Init(AppConfig_TypeDef*);
/* USER CODE END PFP */

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

	SystemClock_Config();

	// Clear the LCD screen and display a welcome message

	/* USER CODE END Init */

	/* Configure the system clock */

	/* USER CODE BEGIN SysInit */

	//__HAL_RCC_CRC_CLK_ENABLE();
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_USART2_UART_Init();
	MX_TIM3_Init();
	MX_USART6_UART_Init();
	/* USER CODE BEGIN 2 */

	printf("Start Hand Posture Sensing..\r\n");

	/* Perform HW configuration (sensor) related to the application  */
	Sensor_Init(&App_Config);

	/* Perform SW configuration related to the application  */
	Software_Init(&App_Config);

	/* Initialize the Neural Network library  */
	Network_Init(&App_Config);

	/* Start communication */
	Comm_Start(&App_Config);

	/* Hand gesture lib init */
	gesture_library_init_configure();

	/* Sensor ranging start */
	Sensor_StartRanging(&App_Config);

	printf("Sensor init done..\r\n");

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
	    Comm_HandleRxCMD(&App_Config);

	    if (App_Config.app_run)
	    {
	        Sensor_GetRangingData(&App_Config);

	        static uint8_t lastPrintedPosture = 0xFF;
	        static uint32_t lastPosturePrintTime = 0;
	        static GW_label_t lastPrintedGesture = GW_NONE;
	        static uint32_t lastGesturePrintTime = 0;
	        const uint32_t PRINT_COOLDOWN_MS = 2000;

	        uint32_t currentTime = HAL_GetTick();

	        if (App_Config.new_data_received)
	        {

	            Network_Preprocess(&App_Config);

	            Network_Inference(&App_Config);

	            Network_Postprocess(&App_Config);

				Add_SensorTxData_Network(&App_Config, &SensorTxData);

				SEN_CopyRangingData(&sensor_data, &App_Config.RangingData);

				GW_run(&gest_predictor, &hand_tracker, &sensor_data);

				Add_SensorTxData_Gesture(&SensorTxData);

	            static uint32_t lastSpeedPrintTime = 0;
	            const uint32_t SPEED_PRINT_COOLDOWN_MS = 100; // Print speeds every 100ms


	            // --- Hand Posture Handling with Cooldown ---
	            if (App_Config.AI_Data.is_valid_frame &&
	                SensorTxData.PostureVData[0] >= MIN_POSTURE_CONFIDENCE)
	            {
	                uint8_t detectedHandPosture = App_Config.AI_Data.handposture_label;

	                if (currentTime - lastPosturePrintTime > PRINT_COOLDOWN_MS)
	                {
	                    switch (detectedHandPosture)
	                    {
	                        case 2: printf("Like\r\n"); break;
	                        case 3: printf("Love\r\n"); break;
	                        case 4: printf("Dislike\r\n"); break;
	                        case 5: printf("BreakTime\r\n"); break;
	                        case 6: printf("CrossHands\r\n"); break;
	                        case 7: printf("Fist\r\n"); break;
	                        default: break;
	                    }

	                    lastPrintedPosture = detectedHandPosture;
	                    lastPosturePrintTime = currentTime;
	                }
	            }

	            // --- Swipe Gesture Handling with Cooldown ---
	            if (gest_predictor.gesture.ready && gest_predictor.lc_state != GW_NONE)
	            {
	                GW_label_t detectedGesture = gest_predictor.gesture.label;

	                if (currentTime - lastGesturePrintTime > PRINT_COOLDOWN_MS)
	                {
	                    switch (detectedGesture)
	                    {
	                        case GW_LEFT:     printf("Swipe Left\r\n"); break;
	                        case GW_RIGHT:    printf("Swipe Right\r\n"); break;
	                        case GW_DOWN:     printf("Swipe Down\r\n"); break;
	                        case GW_UP:       printf("Swipe Up\r\n"); break;
	                        case GW_TOWARD:   printf("Tap Toward\r\n"); break;
	                        case GW_AWAY:     printf("Away\r\n"); break;
	                        case GW_DOUBLETAP:printf("Double Tap\r\n"); break;
	                        default:          printf("Unknown Gesture: %d\r\n", detectedGesture); break;
	                    }

	                    lastPrintedGesture = detectedGesture;
	                    lastGesturePrintTime = currentTime;
	                }
	            }
	        }
	    }
	}
}
/* USER CODE END 3 */

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

/** Configure the main internal regulator output voltage
 */
__HAL_RCC_PWR_CLK_ENABLE();
__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

/** Initializes the RCC Oscillators according to the specified parameters
 * in the RCC_OscInitTypeDef structure.
 */
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
RCC_OscInitStruct.HSIState = RCC_HSI_ON;
RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
RCC_OscInitStruct.PLL.PLLM = 16;
RCC_OscInitStruct.PLL.PLLN = 336;
RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
RCC_OscInitStruct.PLL.PLLQ = 7;
if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
{
	Error_Handler();
}

/** Initializes the CPU, AHB and APB buses clocks
 */
RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
		| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
{
	Error_Handler();
}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

/* USER CODE BEGIN I2C1_Init 0 */

/* USER CODE END I2C1_Init 0 */

/* USER CODE BEGIN I2C1_Init 1 */

/* USER CODE END I2C1_Init 1 */
hi2c1.Instance = I2C1;
hi2c1.Init.ClockSpeed = 400000;
hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
hi2c1.Init.OwnAddress1 = 0;
hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
hi2c1.Init.OwnAddress2 = 0;
hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
if (HAL_I2C_Init(&hi2c1) != HAL_OK)
{
	Error_Handler();
}
/* USER CODE BEGIN I2C1_Init 2 */

/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void)
{

/* USER CODE BEGIN TIM3_Init 0 */

/* USER CODE END TIM3_Init 0 */

TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
TIM_MasterConfigTypeDef sMasterConfig = { 0 };
TIM_OC_InitTypeDef sConfigOC = { 0 };

/* USER CODE BEGIN TIM3_Init 1 */

/* USER CODE END TIM3_Init 1 */
htim3.Instance = TIM3;
htim3.Init.Prescaler = 84 - 1;
htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
htim3.Init.Period = 100 - 1;
htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
{
	Error_Handler();
}
sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
{
	Error_Handler();
}
if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
{
	Error_Handler();
}
sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
{
	Error_Handler();
}
sConfigOC.OCMode = TIM_OCMODE_PWM1;
sConfigOC.Pulse = 0;
sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
{
	Error_Handler();
}
if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
{
	Error_Handler();
}
if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
{
	Error_Handler();
}
/* USER CODE BEGIN TIM3_Init 2 */

/* USER CODE END TIM3_Init 2 */
HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

/* USER CODE BEGIN USART2_Init 0 */

/* USER CODE END USART2_Init 0 */

/* USER CODE BEGIN USART2_Init 1 */

/* USER CODE END USART2_Init 1 */
huart2.Instance = USART2;
huart2.Init.BaudRate = 115200;
huart2.Init.WordLength = UART_WORDLENGTH_8B;
huart2.Init.StopBits = UART_STOPBITS_1;
huart2.Init.Parity = UART_PARITY_NONE;
huart2.Init.Mode = UART_MODE_TX_RX;
huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
huart2.Init.OverSampling = UART_OVERSAMPLING_16;
if (HAL_UART_Init(&huart2) != HAL_OK)
{
	Error_Handler();
}
/* USER CODE BEGIN USART2_Init 2 */

/* USER CODE END USART2_Init 2 */

}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void)
{

/* USER CODE BEGIN USART6_Init 0 */

/* USER CODE END USART6_Init 0 */

/* USER CODE BEGIN USART6_Init 1 */

/* USER CODE END USART6_Init 1 */
huart6.Instance = USART6;
huart6.Init.BaudRate = 115200;
huart6.Init.WordLength = UART_WORDLENGTH_8B;
huart6.Init.StopBits = UART_STOPBITS_1;
huart6.Init.Parity = UART_PARITY_NONE;
huart6.Init.Mode = UART_MODE_TX_RX;
huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
huart6.Init.OverSampling = UART_OVERSAMPLING_16;
if (HAL_UART_Init(&huart6) != HAL_OK)
{
	Error_Handler();
}
/* USER CODE BEGIN USART6_Init 2 */

/* USER CODE END USART6_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
GPIO_InitTypeDef GPIO_InitStruct = { 0 };
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

/* GPIO Ports Clock Enable */
__HAL_RCC_GPIOC_CLK_ENABLE();
__HAL_RCC_GPIOH_CLK_ENABLE();
__HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_GPIOB_CLK_ENABLE();

/*Configure GPIO pin Output Level */
HAL_GPIO_WritePin(FLEX_SPI_I2C_N_GPIO_Port, FLEX_SPI_I2C_N_Pin, GPIO_PIN_RESET);

/*Configure GPIO pin Output Level */
HAL_GPIO_WritePin(GPIOA, LD2_Pin | PWR_EN_C_Pin, GPIO_PIN_RESET);

/*Configure GPIO pin Output Level */
HAL_GPIO_WritePin(LPn_C_GPIO_Port, LPn_C_Pin, GPIO_PIN_RESET);

/*Configure GPIO pin : FLEX_SPI_I2C_N_Pin */
GPIO_InitStruct.Pin = FLEX_SPI_I2C_N_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(FLEX_SPI_I2C_N_GPIO_Port, &GPIO_InitStruct);

/*Configure GPIO pin : INT_C_Pin */
GPIO_InitStruct.Pin = INT_C_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(INT_C_GPIO_Port, &GPIO_InitStruct);

/*Configure GPIO pins : LD2_Pin PWR_EN_C_Pin */
GPIO_InitStruct.Pin = LD2_Pin | PWR_EN_C_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/*Configure GPIO pin : LPn_C_Pin */
GPIO_InitStruct.Pin = LPn_C_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(LPn_C_GPIO_Port, &GPIO_InitStruct);

/* EXTI interrupt init*/
HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(EXTI4_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */

//Set I2C enable
HAL_GPIO_WritePin(FLEX_SPI_I2C_N_GPIO_Port, FLEX_SPI_I2C_N_Pin, GPIO_PIN_RESET);
// Sensor Reset
HAL_GPIO_WritePin(PWR_EN_C_GPIO_Port, PWR_EN_C_Pin, GPIO_PIN_RESET);
HAL_GPIO_WritePin(LPn_C_GPIO_Port, LPn_C_Pin, GPIO_PIN_RESET);
HAL_Delay(100);
HAL_GPIO_WritePin(PWR_EN_C_GPIO_Port, PWR_EN_C_Pin, GPIO_PIN_SET);
HAL_Delay(100);
HAL_GPIO_WritePin(LPn_C_GPIO_Port, LPn_C_Pin, GPIO_PIN_SET);

/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief Initializes the application context structure
 * @param App_Config_Ptr pointer to application context
 */
static void Software_Init(AppConfig_TypeDef *App_Config_Ptr)
{
/* Initialize application context values */
App_Config_Ptr->Uart_RxRcvIndex = 0;
App_Config_Ptr->Uart_nOverrun = 0;
App_Config_Ptr->UartComm_CmdReady = 0;
App_Config_Ptr->frame_count = 0;
App_Config_Ptr->app_run = true;
App_Config_Ptr->new_data_received = false;
App_Config_Ptr->params_modif = true;

App_Config_Ptr->Params.gesture_gui = 0;
App_Config_Ptr->Params.Resolution = SENSOR__MAX_NB_OF_ZONES;
App_Config_Ptr->Params.RangingPeriod = 50;
App_Config_Ptr->Params.IntegrationTime = 10;

App_Config_Ptr->Params.SensorOrientation = SEN_ORIENTATION;
App_Config_Ptr->Params.ranging_ignore_dmax_mm = GW_MAX_DISTANCE_MM;
App_Config_Ptr->Params.lc_stable_threshold = LC_STABLE_THRESHOLD;
App_Config_Ptr->Params.lc_stable_time_threshold = LC_STABLE_TIME_THRESHOLD;
App_Config_Ptr->Params.lc_maxDistance_mm = LC_MAXDISTANCE_MM;
App_Config_Ptr->Params.lc_minDistance_mm = LC_MINDISTANCE_MM;
App_Config_Ptr->Params.gesture_selection = GW_DETECTION_LIMITATION;
App_Config_Ptr->Params.double_tap_ts_threshold = DOUBLE_TAP_TS_DIFF_THRESHOLD;
App_Config_Ptr->Params.screening_ms = GW_SCREENING_TIME_MS;
App_Config_Ptr->Params.analysis_ms = GW_ANALYSIS_TIME_MS;
App_Config_Ptr->Params.dead_ms = GW_DEAD_TIME_MS;
App_Config_Ptr->Params.closer_mm = GW_CLOSER_DELTA_MM;
App_Config_Ptr->Params.min_speed_x_mm_s = GW_MIN_SPEED_X_MM_S;
App_Config_Ptr->Params.min_speed_y_mm_s = GW_MIN_SPEED_Y_MM_S;
App_Config_Ptr->Params.min_speed_z_mm_s = GW_MIN_SPEED_Z_MM_S;
App_Config_Ptr->Params.max_speed_mm_s = GW_MAX_SPEED_MM_S;
App_Config_Ptr->Params.min_vx_vy = GW_MIN_VX_ON_VY;
App_Config_Ptr->Params.min_vx_vz = GW_MIN_VX_ON_VZ;
App_Config_Ptr->Params.min_vy_vx = GW_MIN_VY_ON_VX;
App_Config_Ptr->Params.min_vy_vz = GW_MIN_VY_ON_VZ;
App_Config_Ptr->Params.min_vz_vx = GW_MIN_VZ_ON_VX;
App_Config_Ptr->Params.min_vz_vy = GW_MIN_VZ_ON_VY;
App_Config_Ptr->Params.min_user_filtering_mm = GW_MIN_USER_FILTERING_MM;
App_Config_Ptr->Params.max_user_filtering_mm = GW_MAX_DISTANCE_MM + GW_FILTERING_AREA_MM;
App_Config_Ptr->Params.filtering_area_mm = GW_FILTERING_AREA_MM;
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
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
	HAL_Delay(200);
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
	HAL_Delay(200);
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
