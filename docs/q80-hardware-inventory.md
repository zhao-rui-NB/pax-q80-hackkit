# Q80 實機硬體盤點

2026-11-21 以唯讀方式檢查這台 Q80 的 `/dev`、`/sys`、`/proc/asound/cards`，並對照本專案 OSAL／XUI 標頭與已測 App。裝置節點代表驅動入口存在，不代表插槽、天線、外接模組或 API 已完成實測。沒有讀取卡片內容、通訊識別碼或安全晶片資料。

| 功能 | 實機證據 | 目前狀態 |
| --- | --- | --- |
| 480×800 LCD、觸控、實體鍵盤 | `/dev/fb`、`/dev/tp`、`/dev/keypad`；input 名稱 `touch_panel`、`keypad` | `hardware_test`、MAINAPP、Sketchpad 已使用 |
| 後鏡頭 | `/dev/video0`，`video1` 連到同一節點；`unicam-camera` 驅動 | `camera_lab`、`pixel_camera` 已完成即時預覽與拍照 |
| 熱感印表機 | `/dev/printer`、`/dev/printer_status` | `printer`、`pixel_camera`、`sketch_print` 已出紙；實體上限每行 384 點 |
| 蜂鳴器與音訊 | `OsBeep()`；ALSA `cs4344` 音效卡和 `/dev/snd/pcmC0D0p` | 蜂鳴器已測；PCM／音樂播放未測，這台 OSAL 未匯出手冊的 `OsPlayAudio()` |
| 磁條讀卡 | `/dev/msr`；`OsMsrOpen()`、`OsMsrSwiped()` | 節點與 API 存在，未插卡／刷卡實測 |
| 接觸式晶片卡 | `/dev/usercard`；`OsIccDetect()`、`OsIccOpen()` | 節點與 API 存在，未插卡實測 |
| 感應讀卡 | `/dev/pcd`；`OsPiccOpen()`、`OsPiccPoll()` | 節點與 API 存在，未感應實測 |
| SAM 卡 | `/dev/samcard1` 至 `/dev/samcard6` | 六個驅動節點；實體插槽與是否有卡未確認 |
| Wi‑Fi、乙太網 | `wlan0`、`eth0`；`OsWifiScan()` | 網路介面存在；掃描與速度未測 |
| 藍牙 | `/dev/bluetooth`、`/dev/ttybt` | 驅動入口存在；無線掃描／配對未測 |
| 通訊模組 | `/dev/modem`、`/dev/ttymodem` | 驅動入口存在；是否裝有行動網路模組／SIM 未確認 |
| USB | `usb1`、`usb2` 根集線器 | 匯流排存在；實體埠及外接裝置未測 |
| RTC 時鐘 | `/dev/rtc0`、`iproc-rtc` | 驅動存在；App 未測 |
| 供電資訊 | `OsCheckPowerSupply()` | `hardware_test` 已顯示供電來源；`OsCheckBattery()` 在此機不支援電量回報 |
| 安全／防拆相關 | `/dev/bbl`、`/dev/sha204` | 僅辨識節點；不作一般展示 App 操作 |

手冊另有 `OsLed()`、`OsGpsOpen()`，但 Q80 的 LED 配置與 GPS 實體硬體尚無證據；`/sys/class/leds`、`/sys/class/power_supply` 皆無項目。不能只憑通用 API 判定這台一定支援。

## 適合下一步驗證的 App

1. **QR／條碼掃描器**：`XuiCreateScaner()`、`XuiScanerDecode()` 與既有鏡頭；需測解碼函式與相機資源切換。
2. **讀卡感測台**：只顯示是否刷過／插入／感應到卡，不保存或顯示卡號、磁條資料或金鑰；分別驗證磁條、晶片與感應讀卡入口。
3. **Wi‑Fi 訊號雷達**：用 `OsWifiScan()` 顯示附近網路訊號強弱；不處理密碼。
4. **聲音實驗室**：先用已驗證的蜂鳴器做節奏／音階，再評估 ALSA PCM 音訊播放。
