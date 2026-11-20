# Q80 MAINAPP 選單

原廠流程是 `pedd → keyman → runapp MAINAPP --system → MAINAPP`。本選單以 `app_main()` DLL 載入，掃描已安裝的 App，每頁顯示五個項目；點選項目後先暫停 XUI，再呼叫原廠 `OsRunApp()`，子 App 結束後恢復選單。`SYSTEM SETTINGS (TM)` 可返回原廠 TM。畫面依照實機參考設計：背景 #080B0F、App 卡片 #FFFFFF、卡片文字 #101010、SCAN #00C934、設定 #FF8700。頁碼與翻頁鍵使用深灰，標題與 App 名稱使用原廠 TM 字型的 XUI 粗體；App ID 保留原本大小寫與底線。

## 建置與部署

先依 [linked_hello](../linked_hello/README.md) 準備 `toolchain/device-libs/libosal.so` 與 `libxui.so`。Telnet 需已啟動。

```powershell
python -m q80 mainapp build
python -m q80 mainapp deploy
```

部署指令會直接更新裝置上的 MAINAPP `.so` 與 `appinfo`。若目前顯示 TM，退出 TM 後由 keyman 啟動選單；若目前已在選單，重啟該選單程序才會載入新版本。

MAINAPP 檔案與目錄的擁有者及群組必須是 `999:999`。原廠 `/usr/bin/runapp` 是 `root:999`、模式 `750`；若錯設成 `999:20003`，DLL MAINAPP 程序便沒有執行 runapp 的群組權限，`OsRunApp()` 會回傳 `-2207`。部署指令只修正 MAINAPP 自己的檔案與目錄，不修改原廠 runapp 權限。

本機編出的普通 ARM 執行檔沒有原廠簽章，這台 Q80 的 kernel 會拒絕啟動。MAINAPP 因此保留已實機驗證可載入的 DLL 形式；不要把 `appinfo` 的 `file_type=dll` 改成一般執行檔設定。

## 實機驗證

在 `999:999` 的 DLL MAINAPP 程序內執行 `OsRunApp("screen_hello", NULL, NULL, NULL, NULL)`，實測回傳 `0`，`OsGetAppExitCode()` 也回傳 `0`。選單中的子 App 必須先以 `app deploy <name>` 安裝；畫面上的項目來自 `OsGetAppInfo()`。
