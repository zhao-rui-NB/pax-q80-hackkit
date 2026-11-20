# Q80 硬體測試 App

`hardware_test` 是直接連結原廠 XUI／OSAL 的 `app_main()` App。畫面依 Q80 的 480×800 螢幕設計；底部觸控按鈕切換分頁，實體 `X／Cancel` 或畫面上的 `EXIT` 退出。

| 分頁 | 操作 |
| --- | --- |
| `KEY` | 按實體鍵，顯示 XUI 鍵碼 |
| `TOUCH` | 點九宮格，各格點過後變色 |
| `LCD` | 檢查紅、綠、藍、白、灰、黑色塊 |
| `BEEP` | 點 1–7，播放對應蜂鳴器音調 |
| `POWER` | 顯示 OSAL 電池級別與供電來源；此機電池電量回報不支援 |

先依 [linked_hello](../linked_hello/README.md) 的步驟，將裝置的 `libosal.so`、`libxui.so` 複製到本機 `toolchain/device-libs/`。之後執行：

```powershell
python -m q80 app build hardware_test
python -m q80 app deploy hardware_test
python -m q80 app run hardware_test
```

按 `X／Cancel` 或畫面 `EXIT` 時，App 會結束，`tm` 選單會自動恢復。若 App 沒有回應，可執行 `python -m q80 app stop hardware_test` 強制停止。
