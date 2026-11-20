# Q80 C API 標頭

此目錄提供 `#include <xui.h>` 與 `#include <osal.h>`。`app build` 和 `app-exe build` 已自動加入此 include 路徑。標頭只提供宣告；函數實作仍在裝置的 `/usr/lib/libxui.so`、`/usr/lib/libosal.so`。

| 標頭 | 怎麼來、做了什麼 | 用途與使用方式 |
| --- | --- | --- |
| `xui.h` | 以社群手工標頭和 XUI 2.1.7 手冊為基礎，對照此機 `libxui.so` 匯出名稱；修正色值順序與部分宣告。手冊 63 個 API 中，收錄此機有同名匯出的 57 個。 | 畫面、字型、圖片、按鍵與觸控。App 使用 `#include <xui.h>`；執行時對接 `/usr/lib/libxui.so`。 |
| `osal.h` | 以社群手工標頭和 OSAL 2.5.3 手冊為基礎，補齊函數宣告並對照此機 `libosal.so` 匯出名稱。手冊約 275 個 API 中，收錄此機有同名匯出的 235 個。 | 系統、網路、印表機、讀卡器、相機與 PED 等。App 使用 `#include <osal.h>`；執行時對接 `/usr/lib/libosal.so`。 |
| `osal_types_extra.h` | 依 OSAL 手冊中函數參數使用的型別補寫，例如相機、Wi-Fi、網路結構與整數別名；把這些宣告從 `osal.h` 分開。 | 讓 `osal.h` 的函數原型可以編譯；`osal.h` 會自動包含它，App 不必單獨 include。沒有對應的 `.so`。 |
| `xui_manual_only.h` | 從 XUI 2.1.7 手冊整理出此機 `libxui.so` 沒有匯出的 6 個 API。 | 比較手冊與裝置版本；此機 App 不應呼叫。 |
| `osal_manual_only.h` | 從 OSAL 2.5.3 手冊整理出此機 `libosal.so` 沒有匯出的 API。 | 比較手冊與裝置版本；此機 App 不應呼叫。 |

函數宣告與型別來自 [Prolin XUI Interface 2.1.7](../../docs/370network_pax-s920/Prolin-XUI-Interface-2.1.7.pdf)、[Prolin API Programming Guide 2.5.3](../../docs/370network_pax-s920/Prolin-API-Programming-Guide-2.5.3.pdf)，以及 [370network/pax-s920 的手工標頭](https://github.com/370network/pax-s920/tree/main/toolchain/paxroot/include)。第三方部分採 MIT 授權，見 `LICENSE.pax-s920`。原標頭有缺漏和錯誤；此處已修正明顯的 PDF 轉錄錯誤與 Q80 色值順序，並對照這台裝置的 ELF 匯出名稱。

**同名匯出只能確認函數名稱存在**，不能證明參數 ABI、裝置硬體或當前系統狀態可用。目前實機呼叫過的 XUI API，包括 `XuiOpen`、`XuiRootCanvas`、`XuiCreateFont`、`XuiCanvasSetBackground`、`XuiCanvasDrawText`、`XuiDestroyFont`、`XuiClose`；例子見 [screen_hello](../../device/apps/screen_hello/README.md)。其他 API 應按需要逐一驗證。手冊版本比此 Q80 函式庫新，因此部分 API 只存在於手冊。

手冊本身也有不一致之處：`PCD_USER_ST` 的欄位在 ARM 上計算為 72 bytes，但手冊註明 76 bytes。因此這類需要傳入結構的 API 仍需取得相符的原廠 SDK 標頭或實機驗證布局後才能安全呼叫。

此 Q80 另有 [手冊外的匯出名稱](q80_extra_exports.md)：XUI 2 個、OSAL 34 個。因缺乏可靠的原型，它們只列名稱，不放進可呼叫標頭。

裝置上其他 `.so`、目前載入情況與印表機缺口見 [裝置函式庫清點](../../docs/device-libraries.md)。

## 螢幕與輸入

- XUI：畫布、背景、文字、圖片、螢幕擷取、實體按鍵、觸控手勢與螢幕鍵盤。
- OSAL：螢幕亮度和大小、鍵盤背光、蜂鳴器、印表機、磁條／IC／感應讀卡器、相機及 PED。
- XUI 不支援多程序同時控制顯示與輸入。範例的 `app run` 會暫停 `tm`；結束後要用 `app stop` 恢復它。PED 狀態也可能改變螢幕、按鍵和觸控的控制權。

一般 Prolin SDK 開發會包含原廠標頭並連結對應 `.so`。目前專案沒有相符版本的原廠 SDK 開發庫；`screen_hello` 使用 `dlopen`／`dlsym`。若先從 Q80 複製原廠 `.so` 到 `toolchain/device-libs/`，則 [linked_hello](../../device/apps/linked_hello/README.md) 可在 `app.toml` 指定 `libraries = ["osal", "xui"]`，編譯時連結、執行時直接呼叫函數。`app-exe` 若直接呼叫 OSAL／XUI，還需要另外處理連結，只有 `#include` 不會自動連上函式庫。
