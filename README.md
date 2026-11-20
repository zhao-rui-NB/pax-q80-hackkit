# PAX Q80 Hackkit

Q80 的電腦端開發與救援工具。指令可用 `python -m q80 <命令>` 執行；只輸入 `python -m q80` 會進入互動模式。

## 準備

- Python 3.11 以上。
- XCB client：`vendor/xcb.exe`，或在設定檔指定位置。

將 `q80.toml.example` 複製為 `q80.toml`，填入裝置位址與 COM 埠。Wi-Fi 使用裝置原廠介面設定。

```powershell
Copy-Item q80.toml.example q80.toml
python -m q80 xcb check
```

編譯 C 程式前，在 Windows 建立 ARM 編譯環境（只需一次）：

```powershell
python toolchain/setup.py
```

腳本會下載 GCC 7.5.0 與 glibc 2.15 sysroot；已自行安裝編譯器者也可透過 PATH 或 `q80.toml` 指定。

## 常用流程

臨時 root 通道只在本次開機有效。`start` 啟動通道後會立即還原原廠 `libosal.so`；每次執行都會覆寫本機 `backups/libosal.so`。

```powershell
python -m q80 aip-root start
python -m q80 aip-root exec "id -u"
python -m q80 aip-root stop
```

需要處理本次開機的 PED 時：

```powershell
python -m q80 aip-root start
python -m q80 ped bypass
python -m q80 aip-root stop
```

若要安裝每次開機執行的 hook：

```powershell
python -m q80 aip-root start
python -m q80 preload install
python -m q80 aip-root stop
python -m q80 xcb reboot
```

要撤銷安裝，將上例的 `preload install` 換成 `preload restore`，再重開機。

## 指令

### 本機操作（不連裝置）

| 命令 | 用途 |
| --- | --- |
| `aip inspect <file>` | 檢查 AIP 內容 |
| `app new <name>` | 在 `device/apps/<name>/` 建立 `app.toml` 與最小 C 範本 |
| `app build <name>` | 編譯 App 的 C 原始碼，產生 `build/<id>/<binary>`（預設為 SO） |
| `app pack <name>` | 將已編好的 SO 與 App 資訊打包成 `build/<id>.aip` |
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
| `app deploy <name>` | 上傳已編好的 SO 與 App 資訊到裝置 |
| `app run <name>` | 暫停 `tm` 並透過 `runapp` 啟動 App |
| `app stop <name>` | 停止 App 並恢復 `tm` |
| `app logs <name>` | 查看 App 的標準輸出 |
| `app list` | 列出裝置上已安裝的 App |
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

需要螢幕顯示範例，可從 [screen_hello](device/apps/screen_hello/README.md) 開始。
想直接連結裝置 XUI／OSAL 庫，可看 [linked_hello](device/apps/linked_hello/README.md)。
