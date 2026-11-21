# Q80 Sketchpad 小畫家

`sketchpad` 使用 XUI 原生簽名畫板，手指可直接在白色畫布畫圖。畫筆採此 API 支援的最大寬度 10 點；觸控筆跡由原生元件處理，主迴圈不逐點重繪。按實體刪除／Clear 鍵或畫面上的 `CLEAR` 會建立新畫布；按實體 `X／Cancel` 或畫面上的 `EXIT` 返回 MAINAPP。畫作只保留於目前畫面，離開後不儲存。

```powershell
python -m q80 app build sketchpad
python -m q80 app deploy sketchpad
```

畫面以 Q80 的 480×800 螢幕設計。App 連結 `osal` 與 `xui`；裝置上的 `libxui.so` 需要由 `libosal.so` 提供 `clock_gettime`，只連結 `xui` 會使 `runapp` 在載入時退出。已在實機確認從 MAINAPP 啟動、連續畫線順暢且夠粗，以及實體 Clear 鍵可清空畫布。
