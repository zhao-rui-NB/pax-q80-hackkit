# PAX Q80 Hackkit

Q80 的電腦端開發與救援工具。這裡是重新整理的實作；`q80-adb` 保留為研究紀錄。

## 使用方式

使用 Python 3.11 以上，在專案根目錄執行 `python -m q80` 進入互動模式，
之後直接輸入 `help`、`xcb`、`shell id` 等命令。
也可以安裝本專案後使用 `q80`，或以 `python -m q80 app new my_app`
執行單次命令。
`python -m q80 --help` 與互動模式的 `help` 會顯示相同的主命令清單。
各層命令也可用 `--help` 查看用法，例如 `python -m q80 app --help`、
`python -m q80 app build --help`、`python -m q80 xcb ls --help`。
CLI 的 help、操作訊息與錯誤訊息使用簡單英文，並以 `[module] ✅／❌` 標示成功與錯誤；
一般資訊與除錯訊息只有 `[module]` 前綴。`python -m q80 --verbose <命令>`
可顯示除錯細節。裝置命令回傳的原始資料保持原樣。
直接從 Python 呼叫 SDK 時，可先執行 `from q80.log import configure; configure()`
啟用相同的終端格式；模組本身使用標準 `logging`，也可交由呼叫端設定輸出。

### 不依賴 Telnet：XCB、AIP 與本機準備

| 命令 | 功能 |
| --- | --- |
| `xcb` | 進入互動控制台，可瀏覽檔案，也可直接輸入原生 XCB 命令 |
| `xcb <xcb 命令>` | 原樣呼叫 XCB client，例如 `xcb devinfo` |
| `xcb check` | 用短逾時的 `gettime` 測試連線；無回應才斷開 COM 並重連 |
| `xcb reconnect` | 斷開設定的 COM 連線、重新連接，再用 `gettime` 確認；裝置尚未就緒時最多嘗試三次 |
| `xcb ls [path]` | 直接列出裝置檔案，預設為 `/` |
| `xcb stat <path>`、`xcb cat <path>`、`xcb pull <remote> <local>` | 直接查詢、讀取或下載裝置檔案 |
| `aip inspect <file>` | 在本機檢查 AIP 內容 |
| `aip install <file>` | 透過 XCB 安裝 App AIP |
| `app new <name>` | 在本機建立 App |
| `app build <name>` | 在本機編譯 App |
| `app pack <name>` | 在本機打包 App AIP |
| `aip-root start` | 經 XCB 安裝暫時的 `libosal.so` root loader；不檢查 Telnet 狀態 |
| `aip-root exec <command>` | 透過 XCB 上傳命令、以 AIP 觸發執行並顯示輸出；失敗時回報退出碼；需先執行 `aip-root start` |
| `aip-root restore` | 還原原廠 `libosal.so`，結束暫時 root 命令通道 |
| `ped bypass` | 經 AIP root 處理本次開機的 BBL／PED 狀態並恢復原廠服務；需先執行 `aip-root start` |
| `preload install` | 經 AIP root 安裝持久開機 hook；需先執行 `aip-root start`，不會自動重開機 |
| `preload restore` | 經 AIP root 還原先前的 preload 與 hook；需先執行 `aip-root start` |

### 目前需要 Telnet：命令、App 執行與狀態查詢

| 命令 | 功能 |
| --- | --- |
| `shell <command>` | 經網路 Telnet 執行命令並回報退出碼 |
| `doctor` | 經 Telnet 查詢裝置狀態 |
| `ped status` | 經 Telnet 查詢 PED 狀態 |
| `preload status` | 經 Telnet 查詢 preload 狀態 |
| `app deploy <name>` | 經 Telnet 傳檔部署 App |
| `app run <name>` | 經 Telnet 執行 App |
| `app stop <name>` | 經 Telnet 停止 App |
| `app logs <name>` | 經 Telnet 查看 App 輸出 |
| `app list` | 經 Telnet 查看已安裝 App |

複製 `q80.toml.example` 為 `q80.toml`，調整裝置位址、COM 埠與工具路徑。
Wi-Fi 設定使用 Q80 原廠 UI；研究與實機測試確認其設定可在重開機後自動連線。
若只需臨時操作網路或啟動 Telnet，可先執行 `aip-root start`，再用
`aip-root exec <command>` 執行所需的裝置指令，完成後執行 `aip-root restore`。
`xcb` 需要可用的 `xcb.exe`；放到 `vendor/` 或在設定檔指定位置。
原始 XCB 命令由 `xcb.exe` 執行；`xcb ls/stat/cat/pull` 和互動控制台的檔案查詢走它的本機 server。
`xcb <xcb 命令>` 原樣交給 XCB client，不會自動連線或加入 `-s`。需要時輸入
`xcb check` 測試並在必要時重連，或以 `xcb reconnect` 強制重連；沒有連線時，直接顯示 XCB 的錯誤訊息。
高階 AIP／救援流程才會自行管理 XCB 連線。

`app deploy` 經 Telnet 傳檔，使用裝置上的 `base64` 與 `sha256sum`，
並在搬到最終位置前核對 SHA-256。這種快速部署的 app 在本次實機重啟後消失；
若要安裝套件，使用 `app pack` 和 `aip install`。AIP 跨重啟保留尚未單獨驗證。
新機尚未啟動 Telnet 時，先使用上面的 XCB 命令流程。

`app build` 不使用 WSL，預設呼叫 PATH 中的 `arm-linux-gnueabi-gcc`。
也可透過 `[tools] compiler` 指定其他路徑。最小 `hello` app 已在實機
透過 `runapp` 正常結束；其他 app 與平台 API 尚未驗證。

## 程式邊界

- `q80/transport/` 管理 Telnet 和 XCB；功能模組從 `Session` 取得連線。
- `q80/system/` 放暫時 root 通道、PED 與 preload。
- `q80/aip.py` 只建立與檢查 ZIP 格式，不處理連線。
- `q80/installer.py` 組合 AIP 與 XCB 安裝流程。
- `device/apps/<name>/` 放每個 app 的 `app.toml` 和 C 原始碼。
- `q80/log.py` 統一模組前綴與 `✅／❌` 訊息；後續程式風格見 `AGENTS.md`。

`preload install` 只安裝 PED 開機 hook 與 Telnet daemon；Wi-Fi 使用原廠 UI 的持久設定，
不要求 Wi-Fi 帳密，也不會於開機時覆寫 Wi-Fi 設定或操作 DHCP／`eth0`。
`q80.toml` 是本機明文設定，已被 `.gitignore` 排除。
執行 `preload install` 前先執行 `aip-root start`，
最後執行 `aip-root restore` 還原原廠 `libosal.so`；程式不會自動開始或結束暫時通道。
安裝會保存上一版 hook 與 preload，`preload restore`
只在有本專案建立的備份標記時執行。
`aip-root start` 會暫時替換 `/usr/lib/libosal.so` 以建立 root 命令通道；
替換前會先將這台裝置的原廠庫備份到本機 `backups/`。完成救援後須執行
`aip-root restore` 或重新開機。即使網路 Telnet 已可用，這個命令仍明確建立 AIP root 通道。
重新開機後暫時通道不再存在；執行 `ped bypass` 前需重新執行 `aip-root start`。
例如 `python -m q80 aip-root exec "id; uname -a"` 會執行一條命令並顯示輸出。
`ped bypass` 會還原原廠 `libosal.so`，讓原廠 UI 載入；後續若要執行 `aip-root exec`，需再次執行 `aip-root start`。
執行時每秒檢查 PED 狀態與 `tm` 是否就緒，各階段最多等待 20 秒；不必固定等滿 40 秒。

## 常用的暫時 root 指令

以下在 Windows PowerShell、專案根目錄依序執行。若 PED 擋住原廠 UI，
先建立暫時 root 通道並執行本次開機的 PED bypass：

```powershell
python -m q80 aip-root start
python -m q80 ped bypass
```

等原廠 UI 恢復後，在 Q80 UI 設定並連上 Wi-Fi。`ped bypass` 已還原原廠
`libosal.so`；若要透過 AIP root 關閉 `eth0`、啟動 2323 埠的 Telnet，需重新建立通道：

```powershell
python -m q80 aip-root start
python -m q80 aip-root exec "/bin/busybox ifconfig eth0 down && (/bin/busybox route del default dev eth0 2>/dev/null || true)"
python -m q80 aip-root exec "/bin/busybox setsid /bin/busybox telnetd -l /bin/sh -p 2323 </dev/null >/dev/null 2>&1"
python -m q80 shell id -u
python -m q80 aip-root restore
```

關閉 `eth0` 的指令也會嘗試移除其預設路由；若原本沒有該路由，仍算完成。
`id -u` 輸出 `0` 代表網路 Telnet 已連上 root shell。
最後一行只還原暫時 root 通道的原廠庫，已啟動的 Telnet daemon 不會因此停止。

Wi-Fi 帳密建議直接在 Q80 UI 設定，UI 會保存設定並在重開機後自動連線。
若用 shell 手動設定，還需準備 `wpa_supplicant` 設定、啟動無線服務及 DHCP；
把密碼直接寫在 `aip-root exec` 命令會留在電腦的命令歷史中，因此這裡不提供帳密一行指令。

## 實機驗證（2026-11-18）

以研究資料中的 Q80 韌體驗證：

- XCB `connect`、原始命令、`ls/stat/cat/pull`、AIP 安裝與卸載成功；下載檔案雜湊與本機相同。
- Windows 原生 ARM GCC 建置的最小 DLL app 可經 Telnet 部署，也可由 AIP 安裝；`runapp` 退出碼為 0。
- 移除舊 `/data/etc/ld.so.preload` 後重啟，Telnet 不再自動啟動。XCB 暫時 root 通道建立成功；PED 處理腳本讓原廠服務恢復正常，並還原原廠 `libosal.so`。
- 新 hook 安裝後連續兩次重啟，root Telnet 自動可用，PED trigger、LCD、鍵盤與觸控狀態皆為 0，`pedd/keyman/tm` 存活。preload 還原與重新安裝也成功，當時最後一次重啟狀態正常。
- 當時的 `libosal.so` SHA-256 與開機前裝置備份一致。

這些結果只證明目前測試韌體的行為。新機首次 Wi-Fi 設定、其他韌體版本與硬體 API 尚未實測。
目前 CLI 的命令解析、`aip-root exec` 輸出回讀已通過離線測試。
`xcb check` 已在實機驗證開機後自動重連及健康連線不重連兩條路徑；
本次 `xcb reconnect` 首次 `connect` 回傳 1、第二次成功並通過 `gettime`，`xcb ls /` 也已實機列檔成功。
PED bypass 的新版流程仍待單獨重新驗證。

## 新版 preload 實機驗證（2026-11-19）

- 安裝不含 Wi-Fi 設定的新版 `preload` 後重開機，Q80 原廠 Wi-Fi 自動取得 DHCP 位址，root Telnet 自動可用。
- 移除自動切換路徑後，`aip-root start`、`preload install`、`aip-root restore` 已在實機只經 XCB／AIP 執行成功；原廠 `libosal.so` 已還原並驗證。`ped bypass` 本次只做離線驗證。
