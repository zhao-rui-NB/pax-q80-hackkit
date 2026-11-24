# 螢幕顯示範例

`screen_hello` 是 Q80 的顯示測試 App。它直接連結原廠 XUI 和 OSAL，顯示螢幕解析度，並以內建字型呈現中文、英文及數字。可點選紅、綠、藍、白、黑色塊，或按實體數字鍵 1–5，切換大色塊並查看色碼。

首次建置前，依 [Windows ARM 編譯環境](../../../toolchain/readme.md) 執行 `python toolchain/setup.py`。

先備妥編譯環境並讓裝置的 Telnet 可連線，再於專案根目錄執行：

```powershell
python -m q80 app build screen_hello
python -m q80 app deploy screen_hello
python -m q80 app run screen_hello
python -m q80 app stop screen_hello
```

點 `EXIT` 或按實體取消鍵即可退出。`app stop` 可從電腦提前停止。

建置時依 `app.toml` 連結 `libxui.so` 和 `libosal.so`。執行時由 Q80 的動態載入器從裝置取得這兩個函式庫，字型使用原廠的 `/usr/font/tm_font`。
