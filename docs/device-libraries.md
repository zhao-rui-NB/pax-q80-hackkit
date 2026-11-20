# Q80 裝置函式庫清點

2026-11-20 從實機讀取 `/usr/lib`、`/base/lib` 的 ELF 檔，並查看當時各程序的 `/proc/*/maps`。表中的「已載入」只代表這次快照；未載入的庫仍可能按需使用。沒有執行外設操作。

## 專有函式庫

| 檔案（位於 `/usr/lib`） | 匯出符號所顯示的用途 | 快照中載入的程序 |
| --- | --- | --- |
| `libxui.so` | XUI 畫面與輸入 | `keyman`、`tm` |
| `libosal.so` | OSAL 系統與外設，包括印表機 | 多個系統服務、`keyman`、`tm` |
| `libProlinKMS.so` | `Kms*` 金鑰管理介面 | `tm` |
| `libProTimsAPI.so` | 終端管理／應用相關介面 | `tm` |
| `libgped.so` | `GPed*` PED 服務介面 | `lkiservice`、`tm` |
| `librki.so` | `Rki*` 金鑰注入介面 | `lkiservice`、`tm` |
| `libvoltage.so` | `VpPos*` 加密與卡片資料介面 | `sred` |
| `libcbinder.so` | `binder_*` 程序間通訊 | 多個系統服務、`keyman`、`tm` |
| `libts-1.0.so.0.0.0` | 觸控輸入 | `ipservice`、`keyman`、`tm` |
| `libauth.so` | `Auth*` 認證／密碼介面 | 未見 |
| `libimdll.so` | `DllIcc*`、`dll_msr*`、`dll_iso14443*` 讀卡介面 | 未見 |
| `libppa.so` | `PPA*` PED／密碼介面 | 未見 |
| `libdecoder.so`、`libSrDecoder.so`、`libzmxf.so` | 條碼／QR 解碼相關符號 | 未見 |

`/usr/lib` 另外有 `libip4tc.so.0.1.0`、`libip6tc.so.0.1.0`、`libiptc.so.0.0.0`、`libxtables.so.10.0.0`（網路封包規則）；這次快照均未見載入。`/usr/lib/ts` 有 5 個觸控過濾插件；`/usr/lib/xtables` 有 8 個封包規則插件。未見載入不代表沒有被使用過。

`/base/lib` 有 glibc 2.15 的載入器與 C 標準庫，另有 OpenSSL、ALSA、FreeType、Fribidi、JPEG、PNG、QR Encode、zlib、libarchive、liblzma、libstdc++ 等。這些是作業系統或上表庫的依賴；不屬於 `xui.h`／`osal.h` 的 API。`/base/lib/gconv` 還有字元編碼插件。

這次快照中，`/base/lib` 已映射的非 glibc 庫是 `libasound`、`libcrypto`、`libfreetype`、`libfribidi`、`libgcc_s`、`libpng12`、`libqrencode`、`libssl`、`libz`；`libarchive`、`libjpeg`、`liblzma`、`libpng.so.3`、`libstdc++` 未見映射。其餘 glibc 元件按程序需求載入。

專案另外部署了 `/data/opt/q80_boot/libq80_boot_hook.so`；它不是原廠庫。`/data/app/screen_hello/libscreen_hello.so` 是專案的範例應用。

## 標頭涵蓋範圍與缺口

目前 `toolchain/include/xui.h`、`osal.h` 只涵蓋這台 Q80 同名匯出的 XUI／OSAL 手冊函數：分別 57、235 個。這不涵蓋上表其他專有 `.so` 的介面，也不保證手冊中的參數型別與這台舊版庫的 ABI 完全相同。OSAL 有 34 個手冊外匯出、XUI 有 2 個，僅在 [匯出清單](../toolchain/include/q80_extra_exports.md) 列名。

印表機沒有獨立的 `libprinter.so`。這台 `libosal.so` 匯出 19 個 `OsPrn*` 函數；實機有 `/dev/printer` 字元裝置，`/dev/printer_status` 連到 SPI 印表機驅動狀態。API 手冊另列 `OsPrnRawData`，但這台庫沒有匯出。檢查本機保存的 Q80 `libosal.so` 後確認 `OsPrnOpen(0, NULL)` 走實體 `/dev/printer` 路徑，故 `osal.h` 已定義 `PRN_REAL=0` 及手冊中的印表機錯誤碼；非零模式的具體用途仍待確認。[printer 測試 App](../device/apps/printer/README.md) 已實機驗證英文文字、字級、等級 4 濃度、384×240 點陣圖、中文點陣圖與走紙。

其他值得按需求補充的介面是掃碼（`libdecoder`／`libSrDecoder`／`libzmxf`）、金鑰管理（`libProlinKMS`／`libgped`／`librki`）、讀卡輔助庫（`libimdll`）、音訊（ALSA）與觸控插件。須先取得可靠的函數原型／結構布局，再建立標頭；僅憑匯出名稱不足以安全呼叫。相機、蜂鳴器、磁條／IC／感應讀卡器的部分高階入口已在 OSAL 標頭。
