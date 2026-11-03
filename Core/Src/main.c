/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>   /* strlen()、strcmp()：算字串長度、比對字串 */
#include <ctype.h>    /* toupper()：小寫轉大寫 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* 摩斯電報器的三種狀態（模式1 按鍵輸入用） */
typedef enum MorseState {
  STATE_IDLE,       /* 待機：沒有按鍵活動，綠燈 */
  STATE_PLAYING,    /* 按鍵觸發的「嗶」聲正在響 */
  STATE_WAIT_CHAR   /* 嗶聲結束，正在計算靜默時間，等判斷字母/單字結束 */
} MorseState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ================= 摩斯碼時間（單位：ms） ================= */
#define DOT_MS          100   /* 點「.」的長度 */
#define DASH_MS         300   /* 劃「-」的長度 */
#define SYMBOL_GAP_MS   100   /* 同一個字母內，點劃之間的間隔 */
#define LETTER_GAP_MS   300   /* 字母與字母之間的間隔（總共） */
#define WORD_GAP_MS     700   /* 單字與單字之間的間隔（總共） */

/* ================= 模式1 按鍵解碼判定（單位：ms） ================= */
#define CHAR_END_MS     700   /* 放開後靜默滿這麼久 → 判定一個字母結束 */
#define WORD_END_MS     1800  /* 放開後靜默滿這麼久 → 判定一個單字結束，送空格 */

/* ================= 按鍵 ================= */
#define DEBOUNCE_MS     200          /* 兩次有效按鍵的最小間隔（防彈跳） */
#define BTN_PORT        GPIOA        /* 兩顆按鍵都在 GPIOA */
#define BTN_DOT_PIN     GPIO_PIN_0   /* PA0 = 點 */
#define BTN_DASH_PIN    GPIO_PIN_1   /* PA1 = 劃 */

/* ================= 緩衝區大小 ================= */
#define RX_BUF_SIZE     32    /* UART 接收緩衝：最多存 31 字 + 1 個 '\0' 結尾 */
#define MORSE_BUF_SIZE  8     /* 一個字母的點劃緩衝：最多 7 個符號 + 1 個 '\0' */

/* ================= UART ================= */
#define UART_TX_TIMEOUT 100   /* 傳送逾時（ms），避免卡死 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef hlpuart1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
/* ---------- UART 接收相關 ---------- */
uint8_t rx_byte;                       /* HAL 每次收到的 1 個位元組放這裡 */
volatile char rx_buf[RX_BUF_SIZE];     /* 收集一整行文字；中斷寫、主迴圈讀 → volatile */
uint8_t rx_idx = 0;                    /* 目前寫到 rx_buf 第幾格（只有中斷用，不需 volatile） */
volatile uint8_t cmd_ready = 0;        /* 旗標：收到 '\n'，一整行準備好了 */

/* ---------- 摩斯碼表（const：放在 Flash，不會被改） ---------- */
const char *morse_letters[26] = {      /* A ~ Z */
  ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....", "..",
  ".---", "-.-",  ".-..", "--",   "-.",   "---",  ".--.", "--.-", ".-.",
  "...",  "-",    "..-",  "...-", ".--",  "-..-", "-.--", "--.."
};
const char *morse_digits[10] = {       /* 0 ~ 9 */
  "-----", ".----", "..---", "...--", "....-",
  ".....", "-....", "--...", "---..", "----."
};

/* ---------- 狀態變數（中斷與主迴圈共用 → 全部 volatile） ---------- */
volatile MorseState state = STATE_IDLE;   /* 目前狀態 */
volatile char     pending_elem = 0;       /* 按鍵中斷記下的 '.' 或 '-'，0 = 沒有 */
volatile uint16_t tone_remaining_ms = 0;  /* 這一聲還要響幾 ms（TIM2 倒數） */
volatile uint16_t silence_ms = 0;         /* 嗶聲結束後已經安靜幾 ms（TIM2 累加） */
volatile uint8_t  char_timeout = 0;       /* 旗標：靜默滿 700ms，該解碼字母 */
volatile uint8_t  space_timeout = 0;      /* 旗標：靜默滿 1800ms，該送空格 */
volatile uint8_t  pc_playing = 0;         /* 1 = 正在播放電腦傳來的字 */
volatile char     morse_buffer[MORSE_BUF_SIZE]; /* 目前輸入中的點劃，例如 ".-" */
volatile uint8_t  morse_len = 0;          /* morse_buffer 裡已有幾個符號 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
/* 先宣告，讓後面任何位置都能呼叫這些函式 */
void set_leds(uint8_t g, uint8_t y, uint8_t b);
void tone_on(void);
void tone_off(void);
void uart_print(const char *s);
const char *char_to_morse(char c);
char morse_to_char(const char *code);
void play_text(const char *text);
void handle_button(uint16_t GPIO_Pin);
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ============================================================
 * set_leds：一次設定三顆燈（1 = 亮、0 = 滅）
 * ============================================================ */
void set_leds(uint8_t g, uint8_t y, uint8_t b)
{
  /* 用 CubeMX 的 Label 名稱，不寫死腳位 */
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,  LED_GREEN_Pin,  g ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, y ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,   LED_BLUE_Pin,   b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================
 * tone_on：蜂鳴器開始響 + LD2 亮
 * ============================================================ */
void tone_on(void)
{
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);                /* PB4 開始輸出 800Hz 方波 */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); /* LD2 亮 */
}

/* ============================================================
 * tone_off：蜂鳴器停止 + LD2 滅
 * ============================================================ */
void tone_off(void)
{
  HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);                   /* 停止 PWM 輸出 */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); /* LD2 滅 */
}

/* ============================================================
 * uart_print：透過 LPUART1 送出一個字串到電腦（只在主迴圈呼叫）
 * ============================================================ */
void uart_print(const char *s)
{
  /* 阻塞式傳送：送完才返回；最多等 UART_TX_TIMEOUT ms */
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)s, (uint16_t)strlen(s), UART_TX_TIMEOUT);
}

/* ============================================================
 * char_to_morse：字元 → 摩斯碼字串；查不到回 NULL
 * 例：'a' → ".-"、'5' → "....."、'!' → NULL
 * ============================================================ */
const char *char_to_morse(char c)
{
  c = (char)toupper((unsigned char)c);           /* 先轉大寫 */
  if (c >= 'A' && c <= 'Z') return morse_letters[c - 'A']; /* 字母：用 c-'A' 當索引 */
  if (c >= '0' && c <= '9') return morse_digits[c - '0'];  /* 數字：用 c-'0' 當索引 */
  return NULL;                                   /* 其他字元：查不到 */
}

/* ============================================================
 * morse_to_char：摩斯碼字串 → 字元；查不到回 '?'
 * 例：".-" → 'A'、"......" → '?'
 * ============================================================ */
char morse_to_char(const char *code)
{
  for (int i = 0; i < 26; i++)                   /* 逐一比對 26 個字母 */
  {
    if (strcmp(code, morse_letters[i]) == 0) return (char)('A' + i);
  }
  for (int i = 0; i < 10; i++)                   /* 逐一比對 10 個數字 */
  {
    if (strcmp(code, morse_digits[i]) == 0) return (char)('0' + i);
  }
  return '?';                                    /* 都對不上 */
}

/* ============================================================
 * play_text：模式2，把一串文字用蜂鳴器 + LED 播成摩斯碼
 * 用 HAL_Delay 阻塞：播放期間主迴圈停在這裡
 * ============================================================ */
void play_text(const char *text)
{
  uint16_t gap = 0;          /* 下一個字母播放前要先等多久（0 = 不用等） */
  uint8_t played_any = 0;    /* 是否已播過至少一個字母（避免開頭就等） */

  pc_playing = 1;            /* 告訴中斷：正在播放，按鍵請忽略 */
  tone_remaining_ms = 0;     /* 中止可能還在響的按鍵音 */
  state = STATE_IDLE;        /* 中止按鍵輸入的狀態 */
  tone_off();                /* 確保一開始是安靜的 */
  set_leds(0, 1, 0);         /* 黃燈：播放中 */

  for (const char *p = text; *p != '\0'; p++)    /* 逐字元處理 */
  {
    if (*p == ' ')                               /* 遇到空白 */
    {
      if (played_any) gap = WORD_GAP_MS;         /* 下一個字母前改等「單字間隔」 */
      continue;                                  /* 空白本身不發聲 */
    }

    const char *code = char_to_morse(*p);        /* 查表 */
    if (code == NULL) continue;                  /* 不認得的字元略過 */

    if (gap > 0) HAL_Delay(gap);                 /* 先等字母間隔或單字間隔 */

    for (const char *s = code; *s != '\0'; s++)  /* 逐一播放點劃 */
    {
      tone_on();                                 /* 開始響 */
      HAL_Delay((*s == '-') ? DASH_MS : DOT_MS); /* 劃響 300、點響 100 */
      tone_off();                                /* 停 */
      if (*(s + 1) != '\0') HAL_Delay(SYMBOL_GAP_MS); /* 不是最後一個符號才等音符間隔 */
    }

    played_any = 1;                              /* 已經播過字母了 */
    gap = LETTER_GAP_MS;                         /* 預設下一個字母前等字母間隔 */
  }

  tone_off();                /* 保險：確定關掉 */
  pending_elem = 0;          /* 清掉播放期間可能殘留的按鍵 */
  char_timeout = 0;          /* 清掉旗標 */
  space_timeout = 0;
  morse_len = 0;             /* 清空按鍵輸入緩衝 */
  morse_buffer[0] = '\0';
  set_leds(1, 0, 0);         /* 綠燈：回待機 */
  pc_playing = 0;            /* 播放結束，恢復接受按鍵 */
}

/* ============================================================
 * handle_button：由 EXTI 中斷呼叫，只做檢查並記錄 pending_elem
 * （不發聲、不傳 UART，真正的處理交給主迴圈）
 * ============================================================ */
void handle_button(uint16_t GPIO_Pin)
{
  static uint32_t last_tick = 0;   /* static：函式結束後仍保留上次有效按鍵的時間 */
  uint32_t now = HAL_GetTick();    /* 開機至今的毫秒數 */
  char elem;

  /* 判斷是哪顆鍵 */
  if (GPIO_Pin == BTN_DOT_PIN)       elem = '.';   /* PA0 → 點 */
  else if (GPIO_Pin == BTN_DASH_PIN) elem = '-';   /* PA1 → 劃 */
  else return;                                      /* 其他腳位不理 */

  /* 防彈跳：距離上次有效按鍵不到 200ms → 忽略 */
  if (now - last_tick < DEBOUNCE_MS) return;

  /* 再讀一次腳位：必須是低電位（真的按著），否則是雜訊 */
  if (HAL_GPIO_ReadPin(BTN_PORT, GPIO_Pin) != GPIO_PIN_RESET) return;

  /* 正在響、正在播放電腦的字、或上一個按鍵還沒處理 → 忽略 */
  if (state == STATE_PLAYING || pc_playing || pending_elem != 0) return;

  last_tick = now;         /* 記下這次有效按鍵的時間 */
  pending_elem = elem;     /* 只記錄，交給主迴圈處理 */
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
  MX_LPUART1_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  tone_off();                                    /* 開機先確保蜂鳴器不響、LD2 滅 */
  set_leds(1, 0, 0);                             /* 綠燈：待機 */
  HAL_TIM_Base_Start_IT(&htim2);                 /* 啟動 TIM2，每 1ms 進一次中斷 */
  HAL_UART_Receive_IT(&hlpuart1, &rx_byte, 1);   /* 開始用中斷接收，收到 1 byte 就通知 */
  uart_print("Morse Telegraph Ready\r\n");       /* 告訴電腦：板子準備好了 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	    /* ==================== 優先：處理電腦送來的命令（模式2） ==================== */
	    if (cmd_ready)
	    {
	      char line[RX_BUF_SIZE];    /* 本地複本，避免播放時 rx_buf 被新資料覆蓋 */
	      uint8_t i;

	      __disable_irq();           /* 暫時關中斷，複製時 UART 中斷不能改 rx_buf */
	      for (i = 0; i < RX_BUF_SIZE - 1 && rx_buf[i] != '\0'; i++)
	      {
	        line[i] = rx_buf[i];     /* 逐字複製 */
	      }
	      line[i] = '\0';            /* 補字串結尾 */
	      cmd_ready = 0;             /* 清旗標 */
	      __enable_irq();            /* 恢復中斷 */

	      if (line[0] != '\0')       /* 空行忽略 */
	      {
	        uart_print("RX:");       /* 先回報收到什麼 */
	        uart_print(line);
	        uart_print("\r\n");
	        play_text(line);         /* 播放（阻塞，播完才往下） */
	        uart_print("DONE\r\n");  /* 告訴電腦：播完了 */
	      }
	    }

	    /* ==================== (b) 字母結束：解碼並送出 ==================== */
	    if (char_timeout)
	    {
	      char code[MORSE_BUF_SIZE]; /* 本地複本，把 volatile 緩衝複製出來再查表 */
	      uint8_t ch;                /* 要送出的字元 */
	      uint8_t i;

	      char_timeout = 0;          /* 清旗標 */
	      set_leds(0, 0, 1);         /* 藍燈：解碼中 */

	      if (morse_len > 0)         /* 有輸入才解碼 */
	      {
	        for (i = 0; i < morse_len; i++) code[i] = morse_buffer[i]; /* 複製點劃 */
	        code[i] = '\0';
	        ch = (uint8_t)morse_to_char(code);                          /* 查表 */
	        HAL_UART_Transmit(&hlpuart1, &ch, 1, UART_TX_TIMEOUT);      /* 送 1 byte */
	      }

	      morse_len = 0;             /* 清空緩衝，準備下一個字母 */
	      morse_buffer[0] = '\0';
	    }

	    /* ==================== (c) 單字結束：送空格、回待機 ==================== */
	    if (space_timeout)
	    {
	      space_timeout = 0;         /* 清旗標 */
	      uart_print(" ");           /* 送一個空格 */
	      set_leds(1, 0, 0);         /* 綠燈：待機（state 已由 TIM2 設為 IDLE） */
	    }

	    /* ==================== (a) 處理新的按鍵 ==================== */
	    if (pending_elem != 0 && state != STATE_PLAYING && !pc_playing)
	    {
	      char e = pending_elem;     /* 取出按鍵 */

	      __disable_irq();           /* 關中斷：下面幾個變數要一起改，不能被 TIM2 插進來 */
	      pending_elem = 0;          /* 清除，表示已經處理 */
	      char_timeout = 0;          /* 新符號開始，取消舊的逾時旗標 */
	      space_timeout = 0;
	      tone_remaining_ms = (e == '-') ? DASH_MS : DOT_MS; /* 這聲要響多久 */
	      state = STATE_PLAYING;     /* 狀態：正在響（TIM2 會開始倒數） */
	      __enable_irq();            /* 恢復中斷 */

	      if (morse_len < sizeof(morse_buffer) - 1)  /* 防溢位：最多存 7 個 */
	      {
	        morse_buffer[morse_len] = e;             /* 存入點或劃 */
	        morse_len++;
	        morse_buffer[morse_len] = '\0';          /* 保持字串結尾 */
	      }

	      set_leds(0, 0, 0);         /* 響的時候三燈全滅（只有 LD2 跟著聲音亮） */
	      tone_on();                 /* 開始響 */
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 169;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 169;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1249;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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
  sConfigOC.Pulse = 625;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|LED_BLUE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin|LED_GREEN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin LED_BLUE_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|LED_BLUE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_YELLOW_Pin LED_GREEN_Pin */
  GPIO_InitStruct.Pin = LED_YELLOW_Pin|LED_GREEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ============================================================
 * EXTI 回呼（按鍵按下的下降緣）
 * G4 的 HAL 只會呼叫 HAL_GPIO_EXTI_Callback；
 * Falling_Callback 是給 G0/U5 等系列用的，寫了方便移植
 * ============================================================ */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  handle_button(GPIO_Pin);   /* 只記錄，不做重活 */
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  handle_button(GPIO_Pin);   /* 只記錄，不做重活 */
}

/* ============================================================
 * TIM2 每 1ms 中斷一次：負責按鍵音倒數與靜默計時
 * ============================================================ */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance != TIM2) return;   /* 只處理 TIM2 */
  if (pc_playing) return;               /* 模式2 播放中由 HAL_Delay 計時，這裡不插手 */

  if (state == STATE_PLAYING)           /* 正在響 */
  {
    if (tone_remaining_ms > 0) tone_remaining_ms--;  /* 倒數 1ms */
    if (tone_remaining_ms == 0)                      /* 響夠了 */
    {
      tone_off();                       /* 停止蜂鳴器與 LD2 */
      silence_ms = 0;                   /* 靜默計時從 0 開始 */
      state = STATE_WAIT_CHAR;          /* 進入等待狀態 */
    }
  }
  else if (state == STATE_WAIT_CHAR)    /* 正在靜默 */
  {
    silence_ms++;                       /* 累加 1ms */
    if (silence_ms == CHAR_END_MS)      /* 剛好滿 700ms */
    {
      char_timeout = 1;                 /* 通知主迴圈：解碼字母 */
    }
    if (silence_ms == WORD_END_MS)      /* 剛好滿 1800ms */
    {
      space_timeout = 1;                /* 通知主迴圈：送空格 */
      state = STATE_IDLE;               /* 回待機，停止計時 */
    }
  }
}

/* ============================================================
 * UART 每收到 1 個 byte 呼叫一次：組成一整行
 * ============================================================ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == LPUART1)       /* 確認是 LPUART1 */
  {
    char c = (char)rx_byte;             /* 剛收到的字元 */

    if (c == '\n')                      /* 一行結束 */
    {
      rx_buf[rx_idx] = '\0';            /* 補字串結尾 */
      rx_idx = 0;                       /* 下一行從頭寫 */
      cmd_ready = 1;                    /* 通知主迴圈 */
    }
    else if (c == '\r')                 /* Windows 換行的 \r */
    {
      /* 略過 */
    }
    else
    {
      if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');   /* 小寫轉大寫 */

      /* 只收 A~Z、0~9、空白；標點、中文直接略過 */
      if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')
      {
        if (rx_idx < sizeof(rx_buf) - 1)   /* 還有空間（保留 1 格給 '\0'） */
        {
          rx_buf[rx_idx] = c;              /* 存入 */
          rx_idx++;
        }
        /* 超過 31 字：直接丟棄 */
      }
    }

    HAL_UART_Receive_IT(&hlpuart1, &rx_byte, 1);   /* 重新啟動，收下一個 byte */
  }
}

/* ============================================================
 * UART 發生錯誤（例如 overrun：資料來太快沒讀走）
 * ============================================================ */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == LPUART1)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);    /* 清除 overrun 旗標 */
    __HAL_UART_CLEAR_NEFLAG(huart);     /* 順便清雜訊錯誤旗標 */
    __HAL_UART_CLEAR_FEFLAG(huart);     /* 順便清框架錯誤旗標 */
    rx_idx = 0;                         /* 這行資料已不完整，丟掉重來 */
    HAL_UART_Receive_IT(&hlpuart1, &rx_byte, 1);   /* 重新啟動接收 */
  }
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
#ifdef USE_FULL_ASSERT
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
