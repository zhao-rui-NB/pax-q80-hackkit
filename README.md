# PAX Q80 Hackkit

**PAX Q80 Hackkit 讓拆機後觸發防拆的 Q80 也能用來做實機研究和原生 App 開發。** 專案支援透過 XCB 取得臨時 root，使用 PED bypass 讓裝置重新進入正常啟動流程，也能安裝可還原的 preload hook，讓 Q80 每次開機自動處理 PED 並啟動 Telnet shell。建立連線後，可從 Windows 編譯、部署和測試使用原廠 XUI／OSAL 的 C 應用程式。

## 專案功能

| 功能 | 用途 |
| --- | --- |
| **XCB 與 AIP** | 檢查裝置連線、瀏覽及傳輸檔案、打包與安裝 AIP。 |
| **臨時 root 與 PED bypass** | 透過 AIP 建立本次開機的臨時 root 通道，供實機維修和研究使用。PED 流程可處理部分 tamper 狀態，包括 secure key 已清除的情況。已清除的金鑰無法由此流程恢復。 |
| **開機 hook 與 Telnet** | 提供可還原的 preload 開機 hook，依設定在開機時處理 PED 並啟動 Telnet。Telnet 可用來執行命令、看 App 日誌、擷取畫面及診斷裝置。 |
| **原生 App 工具鏈** | 在 Windows 建置 ARM 執行檔或 `app_main()` DLL。可部署、啟停 App，也可使用自訂 MAINAPP 選單啟動已安裝的程式。 |

PED bypass 會改變裝置的防拆行為，請在有權操作的維修或研究設備上使用。實機硬體與目前驗證範圍見 [Q80 實機硬體盤點](docs/q80-hardware-inventory.md)。

## 範例 App

| App | 內容 |
| --- | --- |
| [Q80 App Menu（MAINAPP）](device/apps/mainapp/README.md) | 自訂 MAINAPP 選單，可啟動已安裝的 App、返回原廠設定。 |
| [Display Test](device/apps/screen_hello/README.md) | 顯示解析度，測試內建字型的中英文、數字和色彩。 |
| [Sketchpad](device/apps/sketchpad/README.md) | 使用原生觸控畫板手繪。 |
| [Hardware Test](device/apps/hardware_test/README.md) | 分頁測試按鍵、觸控、LCD、蜂鳴器與電源資訊。 |
| [Camera Lab](device/apps/camera_lab/README.md) | 後鏡頭即時預覽、拍照與本機儲存。 |
| [Printer Lab](device/apps/printer/README.md) | 測試文字、黑白點陣圖、濃度和走紙。 |
| [Sketch & Print](device/apps/sketch_print/README.md) | 將手繪圖案印成黑白小票。 |
| [Pixel Camera](device/apps/pixel_camera/README.md) | 將照片轉成黑白預覽，並用內建熱感印表機列印。 |
| [Label Maker](device/apps/label_maker/README.md) | 大千注音、繁中標籤預覽、字級調整、直印／橫印和手動走紙。 |

## Getting Started

第一次使用本專案時，先在 Windows 準備工具並確認 XCB 連線，再依 Q80 目前的狀態決定是否需要開機 hook。以下用 Display Test 驗證建置、部署和執行。

### 1. 準備 Windows 與連線

需要 Python 3.11 以上。Q80 的 Wi-Fi 可從原廠介面設定。從專案根目錄執行：

```powershell
Copy-Item q80.toml.example q80.toml
```

在 \`q80.toml\` 填入 Q80 的 IP、Telnet 埠及 XCB 使用的 COM 埠。XCB 程式預設使用 \`vendor/xcb.exe\`。

```powershell
python -m q80 xcb check
python -m pip install tqdm
python toolchain/setup.py
```

Setup 會準備 GCC 7.5.0 和 glibc 2.15 sysroot。編譯用的原廠 \`libxui.so\` 和 \`libosal.so\` 已隨專案提供。詳見 [Windows ARM 編譯環境](toolchain/readme.md)。

### 2. 確認 Telnet

```powershell
python -m q80 shell "id -u"
```

如果 Telnet 已經可用，可直接跳到第 3 步。新機若需要讓開機流程處理 PED 並啟動 Telnet，可在實機維修或研究環境安裝本專案的開機 hook：

```powershell
python -m q80 aip-root start
python -m q80 preload install
python -m q80 aip-root stop
python -m q80 xcb reboot
```

等 Q80 重開並重新連上網路後，確認 hook 與 PED 狀態：

```powershell
python -m q80 preload status
python -m q80 ped status
```

若只需要處理本次開機的 PED，可執行 \`aip-root start\`、\`ped bypass\`、\`aip-root stop\`。PED bypass 會停用 tamper response，已清除的金鑰無法由此流程恢復。臨時 root 通道只在本次開機有效，\`aip-root start\` 會更新本機的 \`backups/libosal.so\`。若要撤銷開機 hook，重新開啟臨時 root 通道，執行 \`preload restore\`，關閉通道後重開機。詳見 [AIP root、PED 與 preload 技術流程](docs/technical-flows.md)。

### 3. 建置、部署並執行第一個 App

```powershell
python -m q80 app build screen_hello
python -m q80 app deploy screen_hello
python -m q80 app run screen_hello
```

Q80 應顯示 **Display Test**。點 \`EXIT\` 或按實體取消鍵可退出。若需要從 Windows 結束程式，可執行 \`python -m q80 app stop screen_hello\`；若啟動失敗，可用 \`python -m q80 app logs screen_hello\` 查看日誌。

想從 Q80 上的選單直接啟動已安裝的 App，可另外安裝 [Q80 App Menu（MAINAPP）](device/apps/mainapp/README.md)。

## 指令

指令使用 `python -m q80 <命令>` 執行。只輸入 `python -m q80` 可進入互動模式。

### 本機操作（不連裝置）

| 命令 | 用途 |
| --- | --- |
| `aip inspect <file>` | 檢查 AIP 內容 |
| `app new <name>` | 在 `device/apps/<name>/` 建立 `app.toml` 與最小 C 範本 |
| `app build <name>` | 編譯 App 的 C 原始碼，產生 `build/<id>/<binary>`（預設為 SO） |
| `app pack <name>` | 將已編好的 SO 與 App 資訊打包成 `build/<id>.aip` |
| `mainapp build` | 編譯可啟動其他 App 的 MAINAPP 選單 |
| `app-exe build <source.c> <output>` | 編譯有 `main()` 的 ARM 執行檔 |

### XCB 操作（不需要 Telnet）

| 命令 | 用途 |
| --- | --- |
| `xcb` | 互動控制台 |
| `xcb check` | 檢查 XCB 連線 |
| `xcb reconnect` | 重建 XCB 連線 |
| `xcb ls [path]` | 列出裝置目錄 |
| `xcb stat <path>` | 查看裝置檔案資訊 |
| `xcb cat <path>` | 輸出裝置檔案內容 |
| `xcb pull <remote> <local>` | 下載裝置檔案 |
| `xcb push <local> <remote>` | 上傳本機檔案到裝置 |
| `xcb <原生命令>` | 直接交給 `xcb.exe`，例如 `xcb reboot` |
| `aip install <file>` | 安裝 AIP |
| `aip-root start` | 建立本次開機可重複使用的 root 通道 |
| `aip-root exec <command>` | 透過 root 通道執行命令 |
| `aip-root stop` | 關閉 root 通道 |
| `ped bypass` | 處理本次開機的 PED；需先 `aip-root start` |
| `preload install` | 安裝開機 hook；需先 `aip-root start` |
| `preload restore` | 還原安裝前的 preload 設定；需先 `aip-root start` |

### 網路 Telnet 操作（需先啟動 Telnet）

| 命令 | 用途 |
| --- | --- |
| `shell <command>` | 透過 Telnet 執行命令 |
| `doctor` | 檢查裝置狀態 |
| `ped status` | 查看 BBL 與三個 PED 裝置節點的狀態 |
| `preload status` | 查看開機 hook 與 runtime 狀態 |
| `app deploy <name>` | 透過 Telnet 上傳已編好的 SO 與 App 資訊；若 App 有 `runtime/` 資源，另用 XCB 傳送 |
| `app run <name>` | 暫停 `tm` 並透過 `runapp` 啟動 App |
| `app stop <name>` | 停止 App 並恢復 `tm` |
| `app logs <name>` | 查看 App 的標準輸出 |
| `app list` | 列出裝置上已安裝的 App |
| `mainapp deploy` | 透過 Telnet 部署 MAINAPP 選單 |
| `screenshot <output.png>` | 透過 Telnet 擷取螢幕並存成 PNG |

使用 `python -m q80 --help` 或在命令後加 `--help` 查看完整參數。

擷取螢幕前安裝 Pillow：`python -m pip install pillow`。例如 `python -m q80 screenshot build/screen.png`。截圖使用實機 R／G／B 測試確認的色彩順序。

## 專案結構

- `q80/cli.py`：命令列與互動模式入口。
- `q80/transport/`：XCB 與 Telnet 連線。
- `q80/system/`：臨時 root、PED 與開機 hook 流程。
- `q80/aip.py`、`q80/installer.py`：AIP 打包與安裝。
- `device/`：在 Q80 執行的 C 程式與 shell 腳本。
- `toolchain/`：Windows 編譯環境的下載與解壓腳本。
- `vendor/`：XCB client 與 BusyBox；`tests/`：本機測試。

## 技術流程

[AIP root、PED 與 preload 技術文檔](docs/technical-flows.md) 說明執行順序、檔案路徑與流程圖。

有 `main()` 的 ARM 執行檔使用 `app-exe build`；`app_main` DLL 使用 `app build`。工具鏈來源與實機啟動方式見 [Windows main() 編譯](docs/windows-main.md)。
