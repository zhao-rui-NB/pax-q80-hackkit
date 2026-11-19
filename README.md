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
| `aip-root start` | 經 XCB／AIP 啟動本次開機的 root 命令程序，隨即還原原廠 `libosal.so`；不檢查 Telnet 狀態 |
| `aip-root exec <command>` | 透過 XCB 上傳命令並由常駐程序執行、顯示輸出與退出碼；需先執行 `aip-root start` |
| `aip-root stop` | 關閉本次開機的 root 命令通道；原廠 `libosal.so` 已由 `start` 還原 |
| `ped bypass` | 經 root 命令程序處理本次開機的 BBL／PED 狀態並恢復原廠服務；需先執行 `aip-root start` |
| `preload install` | 經 root 命令程序安裝持久開機 hook；需先執行 `aip-root start`，不會自動重開機 |
| `preload restore` | 經 root 命令程序還原先前的 preload 與 hook；需先執行 `aip-root start` |

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
`aip-root exec <command>` 執行所需的裝置指令，最後執行 `aip-root stop`。
`start` 會自動還原原廠庫。
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
- `q80/system/aip_root.py` 集中處理暫時 root 通道的啟動、命令執行、停止及 SO 備份還原；同目錄也有 PED 與 preload 流程。
- `q80/aip.py` 只建立與檢查 ZIP 格式，不處理連線。
- `q80/installer.py` 組合 AIP 與 XCB 安裝流程。
- `device/apps/<name>/` 放每個 app 的 `app.toml` 和 C 原始碼。
- `q80/log.py` 統一模組前綴與 `✅／❌` 訊息；後續程式風格見 `AGENTS.md`。

`preload install` 只安裝 PED 開機 hook 與 Telnet daemon；Wi-Fi 使用原廠 UI 的持久設定，
不要求 Wi-Fi 帳密，也不會於開機時覆寫 Wi-Fi 設定或操作 DHCP／`eth0`。
`q80.toml` 是本機明文設定，已被 `.gitignore` 排除。
執行 `preload install` 前先執行 `aip-root start`；命令程序在本次開機持續運行，
原廠 `libosal.so` 已由 `start` 還原。
安裝會保存上一版 hook 與 preload，`preload restore`
只在有本專案建立的備份標記時執行。
`aip-root start` 會暫時替換 `/usr/lib/libosal.so` 以啟動 root 命令程序；
替換前會先將裝置目前的庫存到本機 `backups/libosal.so`，程序啟動後立即用這份檔案還原。
每次執行 `start` 都會覆寫這份備份；若要保留舊版本，請先自行複製。
若 `start` 中途失敗，程式也會嘗試還原原廠庫；若還原失敗，錯誤訊息會指出本機備份路徑並提示重新開機。
即使網路 Telnet 已可用，`start` 仍明確建立 XCB root 通道。`aip-root stop` 會關閉此通道；
重新開機後命令程序也不再存在。要再次使用時，重新執行 `aip-root start`。
例如 `python -m q80 aip-root exec "id; uname -a"` 會執行一條命令並顯示輸出。
`ped bypass` 會讓原廠服務接手；常駐命令程序仍可供後續 `aip-root exec` 使用。
執行時每秒檢查 PED 狀態與 `tm` 是否就緒，各階段最多等待 20 秒；不必固定等滿 40 秒。

### AIP root 的檔案與執行流程

`start` 的各個階段如下；本機臨時目錄在命令結束時由 Python 刪除，`backups/` 與建置輸出會保留。

1. **備份與建置：**本機編譯 `device/aip_root/root_cmd.c` 為 `build/support/libosal_root_cmd.so`。XCB 從裝置 `/usr/lib/libosal.so` 下載目前的庫，直接存為本機 `backups/libosal.so`；下一次 `start` 會覆寫它。
2. **安裝暫時入口：**本機暫存 `busybox.aip` 與 `root_cmd.aip`。前者的 `probe` 符號連結指向裝置 `/bin`，套件內有 `probe/busybox` 與 `probe/sh`；後者的 `manifest.xml` 符號連結指向 `/usr/lib`，套件內有 `manifest.xml/libosal.so`。XCB 呼叫 `installer aip` 後，依套件路徑將 BusyBox 放在 `/bin/busybox`，並暫時以 root loader 取代 `/usr/lib/libosal.so`。
3. **啟動常駐程序：**XCB 上傳 `device/aip_root/daemon.sh` 到 `/tmp/q80_root_daemon.sh`，再建立 `/tmp/q80_root_spawn` 並呼叫 installer 處理空 AIP。loader 的 constructor 刪除舊就緒檔並建立獨立程序。程序將自己的 PID 寫到 `/tmp/q80_root_ready`，電腦確認該檔案出現。
4. **立即還原原廠庫：**XCB 將本機備份上傳到 `/tmp/q80_libosal_stock.so`。常駐程序透過下述 `exec` 檔案通道執行還原腳本，先寫 `/usr/lib/libosal.so.new`，設定模式為 `755`，再移到 `/usr/lib/libosal.so`。最後嘗試用 `installer uaip` 移除兩個暫時 AIP 的登錄資料。

後續每次 `exec` 都只走 XCB：電腦上傳 `/tmp/q80_task_<代號>.sh`，再依序上傳含代號的 `/tmp/q80_root_request` 及 `/tmp/q80_root_go`。常駐程序看到觸發檔後執行腳本，把標準輸出與錯誤合併寫入 `/tmp/q80_output_<代號>`，刪除本次腳本和請求檔，最後把退出碼寫入 `/tmp/q80_status_<代號>`。電腦以狀態檔判定完成並下載輸出。每次代號不同；電腦端的 `.q80-root.lock` 防止多個命令同時覆蓋請求檔。`exec` 不再呼叫 installer。

`stop` 經同一通道刪除 `/tmp/q80_root_ready`；常駐程序在本次命令結束後退出。`start` 建立的原廠庫備份、建置輸出，以及裝置 `/tmp` 中的部分輸出／狀態檔不會由 `stop` 清除。

若 `ps` 只顯示 `PID USER COMMAND` 表頭，原因可能是裝置的 `/proc` 尚未掛載。`aip-root` 不會改變掛載狀態；本次實機是在手動掛載 procfs 後確認 `ps` 能列出程序。

## 常用的暫時 root 指令

以下在 Windows PowerShell、專案根目錄依序執行。若 PED 擋住原廠 UI，
先建立暫時 root 通道並執行本次開機的 PED bypass：

```powershell
python -m q80 aip-root start
python -m q80 ped bypass
```

等原廠 UI 恢復後，在 Q80 UI 設定並連上 Wi-Fi。此時仍可用同一條 XCB root
通道關閉 `eth0`、啟動 2323 埠的 Telnet：

```powershell
python -m q80 aip-root exec "/bin/busybox ifconfig eth0 down && (/bin/busybox route del default dev eth0 2>/dev/null || true)"
python -m q80 aip-root exec "/bin/busybox setsid /bin/busybox telnetd -l /bin/sh -p 2323 </dev/null >/dev/null 2>&1"
python -m q80 shell id -u
python -m q80 aip-root stop
```

關閉 `eth0` 的指令也會嘗試移除其預設路由；若原本沒有該路由，仍算完成。
`id -u` 輸出 `0` 代表網路 Telnet 已連上 root shell。
`aip-root start` 已還原原廠庫；已啟動的 Telnet daemon 不受其影響。

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
- 移除自動切換路徑後，`aip-root start`、`preload install` 與原廠庫還原流程已在實機只經 XCB／AIP 執行成功；原廠 `libosal.so` 已還原並驗證。`ped bypass` 本次只做離線驗證。
- 常駐命令程序版本的 `aip-root start` 已在實機完整執行：啟動程序後自動還原原廠 `libosal.so`；後續 `aip-root exec id -u` 經 XCB 回傳 `0`，不再觸發 installer。非零退出碼 `7` 正確回報；原廠庫還原後 root 命令仍可使用。當時的版本有做雜湊核對，現版本已移除。
- `aip-root stop` 已在實機驗證：就緒檔與常駐程序消失，隨後的 `aip-root exec` 立即回報需要重新執行 `start`。
- 實機觀察到未掛載的 `/proc` 令 `ps` 只印出表頭；手動掛載 procfs 後，`ps` 正常列出系統與 root daemon 程序。移除 loader 命令通道後的精簡版 `start` 也已在實機完成自動還原與命令執行。
- 固定備份檔版本已在實機執行成功：`start` 寫入 `backups/libosal.so` 並完成還原，後續 `aip-root exec "id -u"` 回傳 `0`。
