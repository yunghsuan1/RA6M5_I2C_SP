# RA6M5 單板 I2C 一主多從 (Multi-Slave) 雙向通訊控制與 TSN 溫度遙測系統

本專案實作於瑞薩 (Renesas) RA6M5 微控制器，在**單塊開發板上**透過硬體迴路 (Loopback)，將 **IIC0 (Master)**、**IIC1 (Slave 1)** 與 **IIC2 (Slave 2)** 連接於同一條實體 I2C 共享匯流排上，建立基於自訂封包格式及 CRC8 校驗的安全傳輸協定。

### 核心功能
*   **LED 控制與查詢 (Slave 1 @ 0x4A)**：Master 透過 I2C 指令控制 Slave 1 的三色 LED 狀態（恆亮、熄滅、閃爍），並能主動查詢其真實狀態。
*   **真實 TSN 溫度遙測 (Slave 2 @ 0x4B)**：Slave 2 透過暫存器級別操作，直接控制內部 12-bit ADC0 與溫度感測器 (TSN)，讀取出廠工廠校準資料並計算出攝氏溫度。Master 可動態切換位址讀取 Slave 2 的溫度封包。
*   **I2C 總線掃描器 (Bus Scanner)**：Master (IIC0) 可發起 `scan` 指令，遍歷 `0x08 ~ 0x77` 的 7-bit 位址，透過發送 Dummy byte 探測總線上活動的從機（如 0x4A 與 0x4B），並統計發現的裝置總數。
*   **視覺化偵錯工具**：配備專屬的 Python Tkinter 雙埠序列偵錯終端機，可實時觀測 Master 與 Slave 兩端的互動細節。

---

## 📌 系統接線定義 (Pin Configuration)

為了在單板上實現 Master 到多個 Slave 的共享匯流排通訊，請按照以下定義將三組 I2C 的 SCL/SDA 訊號線全部並聯：

```mermaid
graph TD
    subgraph EK-RA6M5 開發板 (EK-RA6M5 Board)
        Master[IIC0 Master<br>SCL0: P400<br>SDA0: P401] <--> BusSCL[共享 SCL 總線]
        Master <--> BusSDA[共享 SDA 總線]
        
        Slave1[IIC1 Slave 1 @ 0x4A<br>SCL1: P512<br>SDA1: P511] <--> BusSCL
        Slave1 <--> BusSDA
        
        Slave2[IIC2 Slave 2 @ 0x4B<br>SCL2: P410 / J24-10<br>SDA2: P409 / J24-9] <--> BusSCL
        Slave2 <--> BusSDA
        
        PullUpSCL[4.7K 上拉電阻] ---> BusSCL
        PullUpSDA[4.7K 上拉電阻] ---> BusSDA
        
        Slave1 --> LEDs[板載 RGB LED<br>P006 / P007 / P008]
        Slave2 --> TSN[MCU 內部溫度感測器 TSN<br>暫存器直接控制 ADC0]
    end
    
    PC[PC 偵錯工具 uart_monitor.py] <-->|UART9 / P109/P110| Master
    PC <-->|UART8 / P105/P104| Slave1 & Slave2
```

### 1. I2C 共享匯流排連接
*   **IIC0 (Master)**：SCL0 = `P400`，SDA0 = `P401`
*   **IIC1 (Slave 1)**：SCL1 = `P512`，SDA1 = `P511`
*   **IIC2 (Slave 2)**：SCL2 = `P410` (對應 J24 接頭的 J24-10)，SDA2 = `P409` (對應 J24 接頭的 J24-9)
*   **實體接線方式**：
    *   **SCL 共線**：將 `P400 (SCL0)` ↔ `P512 (SCL1)` ↔ `P410 (SCL2 / J24-10)` 三個引腳用杜邦線連接至同一個共享節點。
    *   **SDA 共線**：將 `P401 (SDA0)` ↔ `P511 (SDA1)` ↔ `P409 (SDA2 / J24-9)` 三個引腳用杜邦線連接至同一個共享節點。
    *   ⚠️ **必須外接上拉電阻**：請在 SCL 共享線上與 SDA 共享線上分別接一個 $4.7\text{ k}\Omega$ 上拉電阻至 3.3V，否則 I2C 訊號將無法拉高，導致通訊中斷。

### 2. 實體 LED 輸出 (由 Slave 1 控制)
*   **藍燈 (Blue LED)**：`P006`
*   **綠燈 (Green LED)**：`P007`
*   **紅燈 (Red LED)**：`P008`

### 3. UART 序列埠控制台 (SCI UART)
*   **Master 終端 (UART9)**：TX = `P109`，RX = `P110`（鮑率 115200, 8N1）
*   **Slave 終端 (UART8)**：TX = `P105`，RX = `P104`（鮑率 115200, 8N1，Slave 1 與 Slave 2 共用此輸出）

---

## 🛠️ 韌體架構與功能 (Firmware Architecture)

### 1. 雙控制台與指令解析 (`uart_console.c`, `ring_buffer.c`)
利用環形緩衝區實作非阻塞的字元接收與發送，支援 Master 控制台 (I2C 通訊轉發) 與 Slave 控制台 (本地暫存器直接讀寫)。

在終端機中，支援以下指令列表：
```text
====== 支援指令列表 ======
  blue [on|off|blink]  - 控制藍燈(P006)
  green [on|off|blink] - 控制綠燈(P007)
  red [on|off|blink]   - 控制紅燈(P008)
  status               - 查詢狀態 (LED / TSN 溫度)
  readreg <reg>        - 讀取特定暫存器值 (Hex/Dec)
  writereg <reg> <val> - 寫入特定暫存器值
  dumpreg              - 傾印所有暫存器狀態
  regmap               - 顯示暫存器圖譜樹狀圖
  tsn                  - 讀取 MCU 真實內部溫度 (TSN，僅限 Master 端執行)
  scan                 - 掃描 I2C 總線裝置 (0x08 ~ 0x77，僅限 Master 端執行)
==========================
```


### 2. I2C 封包通訊協定 (`i2c_packet_protocol.h`, `crc8.c`)
所有通訊皆透過自訂的 5 或 6 位元組封包，並以 CRC8 校驗：
*   **LED 控制指令封包** (共 6 位元組)：
    `Header (0x5A) | Length (0x02) | Command (0x01) | Color (1 byte) | Mode (1 byte) | CRC8 (1 byte)`
*   **LED 狀態回傳封包** (共 5 位元組，由 Slave 1 回應)：
    `Header (0x5A) | BlueState (1 byte) | GreenState (1 byte) | RedState (1 byte) | CRC8 (1 byte)`
*   **TSN 溫度回傳封包** (共 5 位元組，由 Slave 2 回應)：
    `Header (0x5A) | Temp_Integer (1 byte) | Temp_Decimal (1 byte) | Reserved (0x00, 1 byte) | CRC8 (1 byte)`

### 3. Slave 2 與暫存器級別 TSN 溫度計算 (`i2c_slave2.c`)
由於不希望變動複雜的 FSP 圖形配置，Slave 2 採用**直接存取暫存器 (Bare-metal registers)** 的方式控制晶片內部的溫度感測器 (TSN) 與 12-bit ADC0：
*   使用 `R_BSP_MODULE_START(FSP_IP_TSN, 0)` 與 `R_BSP_MODULE_START(FSP_IP_ADC, 0)` 喚醒模組。
*   設定 `ADEXICR = 0x0100U` 選擇 TSN 通道作為 A/D 轉換來源。
*   設定取樣時間暫存器 `ADSSTRT = 0xFF` 確保取樣時間滿足 $\ge 4.15\,\mu\text{s}$ 的硬體安全要求（50 MHz PCLKC 下約 $5.1\,\mu\text{s}$）。
*   利用出廠工廠校準資料暫存器 `R_TSN_CAL->TSCDR`（以 127°C / 3.3V 為基準的 12 位元 ADC 原始值）以及典型斜率 `BSP_FEATURE_TSN_SLOPE` ($4.0\text{ mV/}^\circ\text{C}$)，進行線性插值計算：
    $$\text{Voltage} = \frac{\text{ADC\_Raw} \times 3.3}{4096}$$
    $$\text{Temperature} = \frac{V_s - V_{cal127}}{\text{Slope}} + 127.0^\circ\text{C}$$

### 4. 虛擬暫存器映射與樹狀圖顯示 (`i2c_slave.c`, `uart_console.c`)
為將 LED 控制升級為標準的「I2C 裝置暫存器模擬器」，我們在 Slave 1 整合了一套虛擬暫存器映射，允許透過 I2C 指令對其進行暫存器級別的讀寫。
支援的暫存器分配如下表所示：

| 暫存器位址 | 暫存器名稱 | 權限 | 預設值 | 說明 |
| :--- | :--- | :--- | :--- | :--- |
| `0x00` | `DEVICE_ID` | RO | `0xA5` | 裝置 ID 識別碼 |
| `0x01` | `FW_VERSION` | RO | `0x01` | 韌體版本 |
| `0x02` | `BLUE_STATE` | RW | `0x00` | 藍色 LED 狀態 (`0`: OFF, `1`: ON, `2`: BLINK) |
| `0x03` | `GREEN_STATE`| RW | `0x00` | 綠色 LED 狀態 (`0`: OFF, `1`: ON, `2`: BLINK) |
| `0x04` | `RED_STATE`  | RW | `0x00` | 紅色 LED 狀態 (`0`: OFF, `1`: ON, `2`: BLINK) |
| `0x10` | `RX_PACKETS` | RO | `0x00` | 成功接收的 I2C 自訂通訊封包數 |
| `0x11` | `CRC_FAILS`  | RO | `0x00` | CRC8 校驗失敗次數 |
| `0x12` | `LAST_ERROR` | RO | `0x00` | 上次封包錯誤碼 (`0`:NONE, `1`:BAD_CRC, `2`:BAD_HEADER, `3`:BAD_LEN) |
| `0x13` | `LAST_CMD`   | RO | `0x00` | 上次成功執行的指令 ID (如 `0x01`) |

在 Master 或 Slave 終端機執行 `regmap` 指令時，會印出精美的 Unicode 階層樹狀圖：

```text
Slave 1 (0x4A) Register Map Tree:
├── System Registers
│   ├── [0x00] DEVICE_ID   (RO) = 0xA5  [Device ID]
│   └── [0x01] FW_VERSION  (RO) = 0x01  [Firmware Version]
├── LED Control Registers (RW)
│   ├── [02] BLUE_STATE  (RW) = 0 (OFF)  [Blue LED state]
│   ├── [03] GREEN_STATE (RW) = 0 (OFF)  [Green LED state]
│   └── [04] RED_STATE   (RW) = 0 (OFF)  [Red LED state]
└── Diagnostics & Statistics
    ├── [0x10] RX_PACKETS  (RO) = 0  [Valid custom packets count]
    ├── [0x11] CRC_FAILS   (RO) = 0  [CRC8 verification failures]
    ├── [0x12] LAST_ERROR  (RO) = NONE  [Last packet error state]
    └── [0x13] LAST_CMD    (RO) = 0x00  [Last executed Command ID]
```

#### I2C 鎖死 (Clock Stretch) Bug 修復機制
在實作 I2C Slave 暫存器讀寫時，我們解決了 FSP 驅動在 Slave 傳送長度小於 Master 讀取長度時會無限觸發 empty buffer 中斷導致 CPU 鎖死的硬體 Bug。我們設計了 **5-byte Safety Buffer** 的防護措施，使 Slave 每次觸發 `TX_REQUEST` 時一律寫入 5 個位元組，若為暫存器讀取則首位元組填入暫存器值，後續補零，並在 Master 讀取完第一個位元組後由 Master 隨即發出 NACK+STOP 斷開總線。此機制既保障了傳輸的正確性，也徹底防止了 SCL Clock Stretch 的凍結問題。

---

## 🖥️ PC 端 GUI 偵錯工具 (UI Monitor)

本專案附帶 PC 端視覺化調試程式 `uart_monitor.py`，基於 Python Tkinter 與 `pyserial` 開發。

*   **雙欄獨立顯示**：左右分欄分別連接 Master (UART9) 與 Slave (UART8)，一目了然觀測雙端互動。
*   **連線指示燈**：紅/綠雙色圓形燈顯示當前 COM 埠的連線狀態。
*   **動態重整**：一鍵刷新並檢測目前電腦上可用的 COM 埠，支援多種常見鮑率。
*   **終端控制功能**：
    *   提供「🧹 清除視窗」功能，便於重新觀察特定測試結果。
    *   可選擇是否開啟「自動捲動」。
    *   編碼容錯解碼，防止字元亂碼導致程式崩潰。
*   **便捷發送**：輸入指令後點選「發送」或按下鍵盤 **Enter** 鍵即可送出。

---

## 🚀 開發與驗證順序 (Development & Verification Phases)

### Phase 1 ~ Phase 5 (基礎單一 Master/Slave LED 通訊與 GUI)
*   完成雙向控制台、環形緩衝區、外接上拉電阻硬體排除、以及單一 Slave 1 (0x4A) LED 控制與狀態讀回。

### Phase 6 (多從機共享與實體 TSN Telemetry)
*   **實作**：建立 `i2c_slave2.h` / `i2c_slave2.c`，透過暫存器啟用 TSN 與 ADC0，實作 Master 動態位址切換（`0x4B` ↔ `0x4A`）與整合 `status` / `tsn` 指令。

### Phase 7 (當前階段：I2C 總線掃描器實作與同步修正)
*   **實作**：
    *   在 Master 實作 `i2c_master_probe` 函式，發送 1-byte Dummy 寫入來探測 ACK。
    *   在 Master 終端機加入 `scan` 指令。
    *   在 Slave 2 的回呼函式補上 `I2C_SLAVE_EVENT_RX_REQUEST` 處理以接收並清空掃描產生的 Dummy 封包，維持狀態機同步，解決掃描後第一次讀取 `tsn` 出現 Header 錯誤的問題。
*   **手動驗證流程**：
    1. 在 Master 終端機輸入 `scan`。
       * **預期結果**：顯示 `[SCAN] Found device at 0x4A` 與 `[SCAN] Found device at 0x4B`，以及 `[SCAN] Total devices: 2`。
    2. 掃描後輸入 `status` 或 `tsn`，確認第一次即可成功讀取到 TSN 溫度，且後續 LED 控制皆不受影響。

---

## 📄 檔案目錄結構

*   `uart_monitor.py`：PC 端雙埠串列偵錯 Tkinter 程式
*   `I2C_SP/src/`：RA6M5 開發板原始碼
    *   `hal_entry.c`：主程式進入點，初始化硬體並驅動主輪詢迴圈
    *   `uart_console.h` / `.c`：非阻塞式控制台與 LED 控制邏輯
    *   `ring_buffer.h` / `.c`：基礎環形緩衝區實作
    *   `i2c_packet_protocol.h`：自訂 I2C 封包協定結構與 Command ID 定義
    *   `crc8.h` / `.c`：CRC8 校驗計算函式庫
    *   `i2c_master.h` / `.c`：Master 發送指令、讀取狀態、讀取 TSN 與 I2C 總線掃描 (Probe) 實作
    *   `i2c_slave.h` / `.c`：Slave 1 (LED 控制) 中斷大快取接收、回應狀態與異步列印日誌
    *   `i2c_slave2.h` / `.c`：Slave 2 (TSN 遙測) 暫存器級別溫度計算、封包回應與掃描同步處理
*   `I2C_SP/Debug/src/subdir.mk`：專案編譯相依性設定檔
*   `readme.md`：本說明文件
