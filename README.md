# \# 雙向摩斯電報器（NUCLEO-G474RE 期中專題）

# 

# 斷網時只剩一條線也能溝通：按鍵敲出摩斯碼，板子自動解碼送到電腦；電腦打一行字，板子用聲音和燈光播出來。

# 

# \---

# 

# \##1. 目錄

# 

# 1\. \[情境故事](#1-情境故事)

# 2\. \[替身對照](#2-替身對照)

# 3\. \[功能與驗收條件](#3-功能與驗收條件)

# 4\. \[用到的技術：第 3～7 週勾選表](#4-用到的技術第-37-週勾選表)

# 5\. \[腳位表](#5-腳位表)

# 6\. \[建置與執行](#6-建置與執行)

# 7\. \[程式結構](#7-程式結構)

# 8\. \[實作照片](#8-實作照片)

# 9\. \[AI 使用說明](#9-ai-使用說明)

# 10\. \[已知限制](#10-已知限制)

# 

# \---

# 

# 

# \---

# 

# \## 2. 替身對照

# 

# | 故事裡的模組 | 板上替身 | 說明 |

# |---|---|---|

# | 電報鍵（點） | PA0 外接按鍵 | 按一下 = 一個點 |

# | 電報鍵（劃） | PA1 外接按鍵 | 按一下 = 一個劃 |

# | 電報音響器 | PB4 無源蜂鳴器（800 Hz） | 由 TIM3 PWM 驅動 |

# | 訊號燈 | PA5 LD2（板載） | 與蜂鳴器同步亮滅 |

# | 狀態指示燈 | 綠／黃／藍三顆外接 LED | 待機／播放中／解碼中 |

# | 山下指揮站 | 電腦 Python 程式 `test.py` | 顯示解碼文字、送出要播放的字串 |

# | 通訊線 | LPUART1 經 ST-LINK USB 虛擬 COM 埠 | 115200 8N1 |

# 

# \*\*時間縮放：\*\* 熟練報務員的點長約 60 ms（約 20 WPM）。本專題改用 \*\*點 = 100 ms\*\*（約 12 WPM），讓初學者也按得出、聽得懂。

# 

# \---

# 

# \## 3. 功能與驗收條件

# 

# \### 模式 1：按鍵 → 電腦

# 

# \- 按 PA0 = 點（響 100 ms）、按 PA1 = 劃（響 300 ms），響的時候蜂鳴器與 LD2 同步，三燈全滅。

# \- 放開後靜默滿 \*\*700 ms\*\*：判定一個字母結束，查表解碼，經 UART 送出 1 個字元（查不到送 `?`），藍燈亮。

# \- 靜默滿 \*\*1800 ms\*\*：判定一個單字結束，再送一個空格，回到綠燈待機。

# 

# \### 模式 2：電腦 → 板子

# 

# \- 電腦送出一行文字（以 `\\n` 結尾）。小寫自動轉大寫，只保留 A–Z、0–9 和空白，最多 31 字，空行忽略。

# \- 板子先回 `RX:字串\\r\\n`，接著黃燈亮並播放摩斯碼，播完回 `DONE\\r\\n`，回到綠燈。

# \- 播放期間（`pc\_playing = 1`）按鍵一律忽略。

# 

# | 時間項目 | 長度 |

# |---|---|

# | 點／劃 | 100 ms／300 ms |

# | 音符間 | 100 ms |

# | 字母間（總共） | 300 ms |

# | 單字間（總共） | 700 ms |

# 

# \### 驗收條件

# 

# | # | 動作 | 預期 | 實測 | 結果 |

# |---|---|---|---|---|

# | 1 | 電腦輸入 `sos` | 顯示 `RX:SOS`，黃燈亮，播放 `... --- ...`，顯示 `DONE`，回綠燈 | 待實測 | ☐ |

# | 2 | PA0 按兩下（間隔約 0.3 s） | 約 0.7 s 後電腦顯示 `I`，藍燈亮；1.8 s 後送空格，回綠燈 | 待實測 | ☐ |

# | 3 | 按 PA0 再按 PA1 | 電腦顯示 `A` | 待實測 | ☐ |

# | 4 | 電腦輸入 `hello world`，播放中狂按按鍵 | 無多餘嗶聲、不出現字母，播放完整結束並顯示 `DONE` | 待實測 | ☐ |

# | 5 | 0.2 s 內快速連按 PA0 三下 | 只響一聲，電腦顯示 `E` | 待實測 | ☐ |

# | 6 | 電腦輸入 `Hi, 你好!` | 顯示 `RX:HI`，只播放 HI | 待實測 | ☐ |

# 

# \---

# 

# \## 4. 用到的技術：

# \### GPIO

# 

# \- ☑ \*\*GPIO 輸出\*\*　三色狀態燈與 LD2

# &#x20; PB5 綠、PB10 黃、PA8 藍、PA5 LD2｜`MODER = 01`｜`HAL\_GPIO\_WritePin` 寫 `BSRR` 設／清

# \- ☑ \*\*GPIO 輸入\*\*　點、劃兩顆按鍵

# &#x20; PA0、PA1｜`MODER = 00`（作為 EXTI 來源）、`PUPDR = 01` 上拉｜`HAL\_GPIO\_ReadPin` 讀 `IDR` 確認低電位

# \- ☑ \*\*複用功能 AF\*\*　通訊線與蜂鳴器輸出

# &#x20; PA2／PA3 = LPUART1 TX／RX（AF12）、PB4 = TIM3\_CH1（AF2）｜`MODER = 10`｜`AFR` 選 AF

# 

# \### 時脈

# 

# \- ☑ \*\*PLL 170 MHz\*\*　所有計時與鮑率的共同基準

# &#x20; HSI16 ÷4 ×85 ÷2｜`FLASH\_LATENCY\_4`（4 等待週期）＋ Voltage Scale 1 \*\*Boost\*\*｜AHB／APB1／APB2 皆 ÷1

# 

# \### 中斷

# 

# \- ☑ \*\*外部中斷 EXTI\*\*　按鍵按下即觸發

# &#x20; PA0 → EXTI0、PA1 → EXTI1｜`SYSCFG\_EXTICR1` 選 Port A｜`FTSR1` 下降緣｜`IMR1` 開啟｜`PR1` 寫 1 清除（HAL 處理）

# \- ☑ \*\*NVIC 致能\*\*　四支 ISR 都進得來

# &#x20; `LPUART1\_IRQn`、`TIM2\_IRQn`、`EXTI0\_IRQn`、`EXTI1\_IRQn`

# \- ☑ \*\*優先權與巢狀\*\*　UART 不漏字、計時不被按鍵卡住

# &#x20; Priority Group 4（4 bits 搶佔）｜SysTick = 0、LPUART1 = 1、TIM2 = 2、EXTI0/1 = 3｜高優先可搶佔低優先 ISR

# \- ☑ \*\*軟體去彈跳\*\*　按一下只算一個點或劃

# &#x20; `HAL\_GetTick() - last\_tick ≥ 200 ms` 才接受，並再讀一次腳位必須為低電位

# 

# \### UART

# 

# \- ☑ \*\*UART 傳送\*\*　送出解碼字母、`RX:`、`DONE`

# &#x20; LPUART1 115200 8N1｜`BRR = 256 × 170 MHz ÷ 115200 ≈ 377778`｜`HAL\_UART\_Transmit` 輪詢 `TXE`

# \- ☑ \*\*UART 接收\*\*　接收電腦傳來的一行文字（\*\*使用線性行緩衝，非環形緩衝\*\*）

# &#x20; `HAL\_UART\_Receive\_IT` 每次收 1 byte，RXNE 中斷進 `HAL\_UART\_RxCpltCallback`｜存入 `rx\_buf\[32]`，遇 `\\n` 設 `cmd\_ready`，主迴圈才解析｜ORE 錯誤在 `HAL\_UART\_ErrorCallback` 清除並重啟

# 

# \### Timer／PWM

# 

# \- ☑ \*\*Timer 定時中斷\*\*　按鍵音長度倒數、靜默時間計算

# &#x20; TIM2 `PSC = 169`、`ARR = 999`｜1 ms `UIF`｜靜默 700 次 = 字母結束、1800 次 = 單字結束

# \- ☑ \*\*PWM\*\*　驅動無源蜂鳴器發聲

# &#x20; TIM3 CH1（PB4）`PSC = 169`、`ARR = 1249`、`CCR1 = 625`｜800 Hz、50%｜`CCMR1` PWM mode 1｜`HAL\_TIM\_PWM\_Start/Stop` 切換 `CCER.CC1E`

# \- ☐ \*\*輸入捕捉\*\*

# 

# \*\*合計：用到 12 項，沒用 1 項。\*\*

# 

# \---

# 

# \## 5. 腳位表

# 

# | 腳位 | NUCLEO 排針 | 功能 | CubeMX 設定 | 外接方式 |

# |---|---|---|---|---|

# | PA0 | A0 | 點鍵 | GPIO\_EXTI0、Falling、Pull-up | 按鍵 → GND |

# | PA1 | A1 | 劃鍵 | GPIO\_EXTI1、Falling、Pull-up | 按鍵 → GND |

# | PA2 | （內部） | LPUART1\_TX | AF12 | ST-LINK VCP，免接線 |

# | PA3 | （內部） | LPUART1\_RX | AF12 | ST-LINK VCP，免接線 |

# | PA5 | 板載 | LD2 | GPIO\_Output，Label `LD2` | — |

# | PA8 | D7 | 藍燈 | GPIO\_Output，Label `LED\_BLUE` | 串 220–330 Ω → GND |

# | PB4 | D5 | 蜂鳴器 | TIM3\_CH1（AF2） | 經 NPN 電晶體驅動無源蜂鳴器，基極串 1 kΩ、下拉 10 kΩ |

# | PB5 | D4 | 綠燈 | GPIO\_Output，Label `LED\_GREEN` | 串 220–330 Ω → GND |

# | PB10 | D6 | 黃燈 | GPIO\_Output，Label `LED\_YELLOW` | 串 220–330 Ω → GND |

# | PA13／PA14 | — | SWD | Serial Wire | 保留給燒錄，不使用 |

# 

# \---

# 

# \## 6. 建置與執行

# 

# \### 開發環境

# 

# \- 開發板：NUCLEO-G474RE

# \- STM32CubeMX + STM32CubeIDE（HAL 函式庫）

# \- Windows + Git Bash，Python 3 + pyserial

# 

# \### 燒錄步驟

# 

# 1\. CubeIDE：File → Open Projects from File System → 選本 repo 資料夾。

# 2\. 開啟 `Morse\_code.ioc` 確認設定（見第 4、5 節），必要時按 GENERATE CODE。

# 3\. Build（槌子圖示），確認 0 errors。

# 4\. USB 接上板子，按 Run 燒錄。完成後綠燈亮起，表示待機。

# 

# \### 終端機設定

# 

# \*\*115200 bps、8 資料位元、無同位、1 停止位元（8N1）\*\*

# 

# \#### 方法 A：使用本專題的 `test.py`（建議）

# 

# ```bash

# py -m pip install pyserial

# py test.py            # 自動尋找 STLink / STMicroelectronics 的 COM 埠

# py test.py COM5       # 或手動指定

# ```

# 

# \- 在 `>` 後輸入文字並按 Enter 送出；空輸入不送，輸入 `exit` 離開。

# \- 送出一行後，要等板子回 `DONE` 才能送下一行。

# \- 按鍵解出的字母會即時顯示（沒有換行）。

# \- 在 Git Bash 中若輸入沒有反應，改用 `winpty py test.py`。

# 

# \#### 方法 B：一般終端機（TeraTerm 等）

# 

# 設定 115200 8N1，換行送出 LF（`\\n`）即可。

# 

# \### 通訊協定

# 

# | 方向 | 格式 | 範例 |

# |---|---|---|

# | 電腦 → 板子 | 一行文字 + `\\n` | `sos` |

# | 板子 → 電腦 | `RX:` + 過濾後字串 + `\\r\\n` | `RX:SOS` |

# | 板子 → 電腦 | `DONE\\r\\n` | 播放完成 |

# | 板子 → 電腦 | 1 個字元（無換行） | 按鍵解碼結果，例如 `A` |

# | 板子 → 電腦 | 1 個空格 | 單字結束 |

# 

# \---

# 

# \## 7. 程式結構

# 

# \### 檔案分工

# 

# | 檔案 | 內容 |

# |---|---|

# | `Morse\_code.ioc` | CubeMX 設定（時脈、周邊、腳位、NVIC） |

# | `Core/Src/main.c` | 所有自寫程式，皆在 `USER CODE BEGIN / END` 區塊內 |

# | `Core/Src/stm32g4xx\_it.c` | CubeMX 產生的中斷進入點（`TIM2\_IRQHandler` 等），轉交給 HAL |

# | `Core/Src/stm32g4xx\_hal\_msp.c` | CubeMX 產生的腳位 AF 與 NVIC 優先級設定 |

# | `test.py` | 電腦端測試程式 |

# 

# \### `main.c` 中的 USER CODE 區塊

# 

# | 區塊 | 內容 |

# |---|---|

# | Includes | `string.h`、`ctype.h` |

# | PTD | `MorseState` 列舉 |

# | PD | 所有時間與緩衝大小的 `#define`（要改參數只改這裡） |

# | PV | 全域變數、摩斯碼表 |

# | PFP | 函式宣告 |

# | 0 | `set\_leds`、`tone\_on/off`、`uart\_print`、`char\_to\_morse`、`morse\_to\_char`、`play\_text`、`handle\_button` |

# | 2 | 開機：關蜂鳴器、綠燈、`HAL\_TIM\_Base\_Start\_IT`、`HAL\_UART\_Receive\_IT` |

# | 3 | 主迴圈 |

# | 4 | 所有中斷回呼 |

# 

# \### ISR 與主迴圈分工

# 

# \*\*原則：中斷只設旗標或記錄資料；UART 傳送、解碼、發聲都在主迴圈處理。\*\* 共用變數宣告為 `volatile`；必須一起修改的多個變數用 `\_\_disable\_irq()` / `\_\_enable\_irq()` 保護。

# 

# | 中斷（ISR） | 只做這些事 |

# |---|---|

# | EXTI0 / EXTI1 | `handle\_button()`：去彈跳、檢查狀態，合格才寫入 `pending\_elem` |

# | TIM2（1 ms） | 倒數 `tone\_remaining\_ms`，到 0 就 `tone\_off()`；累加 `silence\_ms`，到 700 設 `char\_timeout`、到 1800 設 `space\_timeout` |

# | LPUART1 RX | 轉大寫、過濾、存入 `rx\_buf`；遇 `\\n` 設 `cmd\_ready` |

# | LPUART1 Error | 清 ORE 等旗標、重啟接收 |

# 

# | 主迴圈依序處理 | 動作 |

# |---|---|

# | `cmd\_ready` | 回 `RX:` → `play\_text()` → 回 `DONE` |

# | `char\_timeout` | 藍燈，解碼並送出 1 個字元 |

# | `space\_timeout` | 送空格，綠燈 |

# | `pending\_elem` | 存入 `morse\_buffer`、全燈滅、`tone\_on()`、`state = STATE\_PLAYING` |

# 

# \### 系統架構

# 

# ```mermaid

# flowchart LR

# &#x20;   BTN\[PA0/PA1 按鍵] -->|EXTI| ISR1\[handle\_button]

# &#x20;   TIM2\[TIM2 1ms] --> ISR2\[計時回呼]

# &#x20;   PC\[電腦 test.py] -->|LPUART1 RX| ISR3\[接收回呼]

# &#x20;   ISR1 -->|pending\_elem| MAIN\[主迴圈]

# &#x20;   ISR2 -->|char\_timeout / space\_timeout| MAIN

# &#x20;   ISR3 -->|cmd\_ready| MAIN

# &#x20;   MAIN -->|TIM3 PWM| BUZ\[蜂鳴器 + LD2]

# &#x20;   MAIN --> LED\[綠/黃/藍燈]

# &#x20;   MAIN -->|LPUART1 TX| PC

# ```

# 

# \### 狀態機（模式 1）

# 

# ```mermaid

# stateDiagram-v2

# &#x20;   \[\*] --> IDLE

# &#x20;   IDLE --> PLAYING: 按鍵

# &#x20;   WAIT\_CHAR --> PLAYING: 按鍵

# &#x20;   PLAYING --> WAIT\_CHAR: 聲音倒數到 0

# &#x20;   WAIT\_CHAR --> WAIT\_CHAR: 靜默 700 ms → 送字母

# &#x20;   WAIT\_CHAR --> IDLE: 靜默 1800 ms → 送空格

# ```

# 

# | 狀態 | 意義 | 燈號 |

# |---|---|---|

# | `STATE\_IDLE` | 待機 | 綠 |

# | `STATE\_PLAYING` | 按鍵觸發的聲音正在響 | 全滅（LD2 亮） |

# | `STATE\_WAIT\_CHAR` | 聲音結束，計算靜默時間 | 解出字母後轉藍 |

# | `pc\_playing = 1` | 播放電腦傳來的字 | 黃 |

# 

# \### 可調參數（`USER CODE BEGIN PD`）

# 

# | 參數 | 預設 | 改大會怎樣 |

# |---|---|---|

# | `DOT\_MS` / `DASH\_MS` | 100 / 300 | 聲音變長、播放變慢 |

# | `SYMBOL\_GAP\_MS` / `LETTER\_GAP\_MS` / `WORD\_GAP\_MS` | 100 / 300 / 700 | 播放停頓變長 |

# | `CHAR\_END\_MS` | 700 | 可以按得更慢，但字母要等更久才出現 |

# | `WORD\_END\_MS` | 1800 | 要等更久才送空格、回待機 |

# | `DEBOUNCE\_MS` | 200 | 更能防彈跳，但不能按太快 |

# | `RX\_BUF\_SIZE` | 32 | 一行可放更多字（字數 = 值 − 1，`test.py` 的 `MAX\_CHARS` 要同步改） |

# 

# 音調：改 CubeMX 中 TIM3 的 ARR（頻率 = 1 MHz ÷ (ARR + 1)），並將 Pulse 設為 (ARR + 1) ÷ 2。

# 

# \---

# 

# \## 8. 實作照片

# 

# | 照片 | 圖說 |

# |---|---|

# | !\[全景](docs/board.jpg) | 全景：NUCLEO-G474RE、麵包板與 USB 線 |

# | !\[接線特寫](docs/wiring.jpg) | 接線特寫：PA0/PA1 按鍵、PB4 蜂鳴器、PB5/PB10/PA8 三色 LED |

# | !\[綠燈](docs/led\_green.jpg) | 待機：綠燈亮 |

# | !\[黃燈](docs/led\_yellow.jpg) | 模式 2 播放中：黃燈亮、LD2 隨聲音閃爍 |

# | !\[藍燈](docs/led\_blue.jpg) | 模式 1 解碼中：藍燈亮 |

# | !\[終端機](docs/terminal.png) | 終端機：`RX:SOS`、`DONE`，以及按鍵解出的字母 |

# 

# \---

# 

# \## 9. AI 使用說明

# 

# 本專題使用 \*\*Claude（Anthropic）\*\* 協助開發。

# 

# \### 我自己決定的部分

# 

# \- 專題題目、兩種模式的規格、時間參數、燈號定義

# \- 腳位分配、CubeMX 各周邊參數、NVIC 優先級

# \- 狀態列舉、全域變數與函式清單、主迴圈 (a)(b)(c) 的處理邏輯、各回呼函式該做的事

# \- 程式規範：只寫在 USER CODE 區塊、中斷只設旗標、共用變數加 `volatile`、使用 CubeMX Label

# 

# \### 請 AI 協助的部分

# 

# | 項目 | AI 做了什麼 |

# |---|---|

# | CubeMX 設定清單 | 依我的規格整理成逐項清單，並說明時脈、Flash Latency、Boost、NVIC 優先級的意義 |

# | `main.c` USER CODE 內容 | 依我指定的函式與邏輯產生程式碼與中文註解 |

# | `test.py` | 依我列的需求（自動找埠、背景執行緒、等 DONE）產生程式 |

# | 除錯 | 協助判斷蜂鳴器停不下來、終端機亂碼、編譯錯誤的原因 |

# | README | 協助撰寫初稿 |

# 

# \### 我怎麼查核

# 

# \- \*\*計時器參數重算：\*\* TIM2：170 MHz ÷ 170 ÷ 1000 = 1 kHz → 1 ms；TIM3：170 MHz ÷ 170 ÷ 1250 = 800 Hz，625 ÷ 1250 = 50%。

# \- \*\*對照產生的程式碼：\*\* 逐一檢查 `MX\_xxx\_Init()` 中的數值是否與 CubeMX 設定一致。

# \- \*\*實測中發現並修正的問題：\*\*

# &#x20; - 蜂鳴器停不下來：TIM2 的 Counter Period 仍是預設的 4294967295（約 71 分鐘才中斷一次），改成 999 後正常。

# &#x20; - 終端機全是問號：LPUART1 鮑率仍是預設的 209700，改成 115200 後正常。

# &#x20; - 編譯錯誤 `invalid storage class for function`：`while(1)` 的右括號放在 USER CODE 區塊外，重新產生程式碼時被刪除，移回 `USER CODE END 3` 之前即解決。

# \- \*\*驗收：\*\* 依第 3 節的驗收條件逐條實測。

# 

# \### Commit 標示

# 

# 由 AI 協助產生的提交，訊息中會註明，例如：

# 

# ```

# feat: 加入 TIM2 計時與按鍵解碼邏輯（AI 協助產生，已實測驗收 #2 #3）

# ```

# 

# \---

# 

# \## 10. 已知限制

# 

# \### 功能限制

# 

# \- \*\*播放為阻塞式：\*\* `play\_text()` 使用 `HAL\_Delay`，播放期間主迴圈停住，按鍵一律忽略。31 字最長約需 70 秒。

# \- \*\*電腦命令優先：\*\* 使用者按鍵輸入到一半時，若電腦傳來字串，會立即播放，輸入中的字母會被清除。

# \- \*\*字元範圍：\*\* 只支援 A–Z、0–9 與空白；標點與中文會被略過。一行最多 31 字。

# \- \*\*輸入速度：\*\* 防彈跳為 200 ms，兩次按鍵間隔必須超過 0.2 秒。按得太慢（間隔超過 0.7 秒）會被拆成兩個字母。

# \- \*\*UART 接收為線性緩衝：\*\* 主迴圈複製後才能接收下一行；播放期間只能再暫存一行，第三行會被覆蓋。

# \- \*\*時間誤差：\*\* `HAL\_Delay(n)` 實際約多等 1 ms，對摩斯碼沒有影響。

# \- \*\*`HAL\_GPIO\_EXTI\_Falling\_Callback`：\*\* G4 系列的 HAL 不會呼叫此函式，實際只會走 `HAL\_GPIO\_EXTI\_Callback`；保留它是為了移植到 G0、U5 等系列時也能使用。

# 

# \### 硬體限制

# 

# \- 蜂鳴器必須是\*\*無源\*\*的。PWM 停止後 PB4 不再被驅動（高阻抗），因此需要下拉電阻，避免電晶體半導通造成雜音。

# 

# \### 換成真實設備要改哪裡

# 

# | 替身 | 換成真實設備 | 要改的地方 |

# |---|---|---|

# | 按鍵 | 真正的電報鍵 | 同樣接 PA0/PA1 到 GND；機械觸點彈跳較大，可能需要加大 `DEBOUNCE\_MS` |

# | 蜂鳴器 | 喇叭 + 功率放大器 | 硬體加放大電路；程式不需更動 |

# | USB 虛擬 COM 埠 | 長距離線路（RS-485）或無線模組 | 加收發器模組，改用 USART 外接腳位；長距離時可能需要降低鮑率 |

# | 電腦 | 另一塊開發板 | 對方板子跑同一份韌體，以 UART 互接 TX/RX |

