# 螢幕顯示範例

`screen_hello` 是使用裝置原廠 XUI 的 `app_main` DLL。畫面顯示 `HELLO Q80` 十秒後結束。

先備妥編譯環境並讓裝置的 Telnet 可連線，再於專案根目錄執行：

```powershell
python -m q80 app build screen_hello
python -m q80 app deploy screen_hello
python -m q80 app run screen_hello
python -m q80 app stop screen_hello
```

執行後會顯示十秒，結束時自動恢復原廠 `tm` 畫面；`app stop` 可提前停止。若啟動失敗，可用 `python -m q80 app logs screen_hello` 查看訊息。

程式使用 [xui.h](../../../toolchain/include/xui.h) 的函數宣告，先用 `dlopen` 將 XUI 需要的裝置函式庫載入，再以 `dlsym` 取得畫字函式；載入順序與字型路徑來自先前的 Q80 實機 XUI probe。程式只使用裝置上的函式庫，沒有把它們打包進 DLL。
