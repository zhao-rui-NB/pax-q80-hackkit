# Q80 打印機測試 App

`printer` 是互動式 OSAL 列印能力測試程式。啟動時不會出紙；只有點選列印項目才呼叫實體印表機。畫面顯示每項 API 的回傳值，`0` 代表成功。`X／Cancel` 或畫面 `EXIT TO MENU` 返回 MAINAPP。

| 按鈕 | 測試內容 |
| --- | --- |
| `CHECK` | `OsPrnOpen`、`OsPrnCheck`、`OsPrnGetDotLine`；不出紙 |
| `FONTS` | 列舉可用的 OSAL 字型；不出紙，完整名稱寫入 App 輸出 |
| `TEXT` | 英文、數字、符號的基本列印 |
| `SIZES` | 三種英文字級：12×24、16×32、24×48 |
| `CHINESE` | 用 XUI 的 `tm_font` 將中文字逐字分三行繪成 384×220 點陣圖，字高 64 點，再透過 `OsPrnPutImage` 列印 |
| `IMAGE` | 以手冊記載的逐行資料格式測試接近全紙寬的 384×240 點陣圖，含大字與方格 |
| `FEED` | 印一行標記並多走紙 96 點 |
| `DENSITY` | 使用 `OsPrnSetGray(4)` 測試手冊列出的最深濃度 |

每次列印先開啟印表機並檢查狀態，錯誤時不送列印內容；成功後以手冊列出的最高濃度等級 4 列印，最後關閉印表機。列印緩衝最多 2048 bytes，測試內容刻意保持短小。

```powershell
python -m q80 app build printer
python -m q80 app deploy printer
```

這台 Q80 的 `libosal.so` 中 `OsPrnOpen(0, NULL)` 會進入 `/dev/printer` 實體裝置路徑，因此在本機 `osal.h` 定義 `PRN_REAL=0`。手冊雖有 `PRN_BMP`，但這台庫包含多種非零模式且數值未確認，本 App 不測虛擬印表機。`OsPrnRawData` 沒有在這台庫匯出。

實機有 `/dev/printer`，權限為 `root:hwdev`、`660`；目前 MAINAPP 程序的補充群組包含 `hwdev`。這只能確認裝置節點可存取，不能代替實際列印測試。測試紙張輸出時應記錄文字是否完整、中文字型、圖片、濃度及走紙效果。

## Q80 實機結果

| 項目 | 回傳與紙張觀察 |
| --- | --- |
| `CHECK` | `OPEN 0 / CHECK 0` |
| `TEXT` | `RESULT 0`，原本濃度在舊紙上偏淡 |
| `DENSITY` | `RESULT 0`；等級 3 比預設深，等級 4 又更深 |
| `SIZES` | `RESULT 0`；三行依序變大 |
| `IMAGE` | `RESULT 0`；384×240 大圖完整出紙 |
| `CHINESE` | `RESULT 0`；64 點單字分三行清楚可讀 |
| `FEED` | `RESULT 0`；標記與額外走紙均正常 |
| `FONTS` | OSAL 僅列出 `paxfont.ttf`，不含本測試所需中文字 |

OSAL `OsPrnSetFont("tm_font")` 回傳 `-1003`，所以中文使用 XUI 字型先繪圖，再由 `OsPrnPutImage` 列印。28 點單行和 44 點兩行太擠或模糊；64 點單字分三行已確認清楚。紙張較舊，最終濃度與細線品質應在換新紙後再比較。尚未測缺紙／過熱錯誤、虛擬 BMP 印表機與手冊中的 `OsPrnRawData`（這台庫沒有匯出）。
