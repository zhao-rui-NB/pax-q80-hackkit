# 直接連結 XUI／OSAL 的螢幕範例

`linked_hello` 使用 `app_main()`，直接呼叫 `Xui...` 和 `OsScrGetSize()`，沒有 `dlopen`／`dlsym`。在 480×800 的 Q80 上顯示 `Q80` 與「Q80 中文字顯示測試」；按任意實體鍵結束。

先在專案根目錄準備編譯時使用的裝置原廠庫：

```powershell
New-Item -ItemType Directory -Force toolchain/device-libs | Out-Null
python -m q80 xcb pull /usr/lib/libosal.so toolchain/device-libs/libosal.so
python -m q80 xcb pull /usr/lib/libxui.so toolchain/device-libs/libxui.so
```

再編譯、部署與啟動：

```powershell
python -m q80 app build linked_hello
python -m q80 app deploy linked_hello
python -m q80 app run linked_hello
python -m q80 app stop linked_hello
```

`app.toml` 的 `libraries = ["osal", "xui"]` 讓編譯器連結這兩份本機庫。產物只記錄 `libosal.so`、`libxui.so` 名稱，執行時由 Q80 載入 `/usr/lib` 的原廠庫；不把庫打包進 App。本機副本已被 Git 忽略。App 自行結束時會恢復原廠 `tm` 畫面；`app stop` 可提前停止。
