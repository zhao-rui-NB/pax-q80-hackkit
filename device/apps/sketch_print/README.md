# Q80 Sketch & Print 畫作小票

`sketch_print` 是獨立於 `sketchpad` 的畫畫列印 App。手指在 440×440 的原生簽名畫板上畫圖；按 `PRINT` 才會把畫作轉成 384×384 點的黑白小票。畫筆採原生 API 支援的最大寬度 10 點，筆跡由 XUI 元件處理。按實體刪除／Clear 鍵或 `CLEAR` 清空畫布，按實體 Cancel 鍵或 `EXIT` 返回 MAINAPP。啟動、畫圖、清空都不會出紙；空白畫布也不會列印。

畫作經 `XuiSigBoardImg()` 擷取，縮至印表機完整紙寬，再以已驗證的 `OsPrnPutImage()` 格式分兩段各 192 行輸出。列印前檢查印表機狀態，使用等級 4 濃度。畫作不會儲存或上傳。

```powershell
python -m q80 app build sketch_print
python -m q80 app deploy sketch_print
```

已在 Q80 實機確認從 MAINAPP 啟動、列印結果 `0` 且筆畫完整；實體 Clear 鍵可清空畫布，空白畫布按 `PRINT` 會顯示 `DRAW SOMETHING FIRST` 且不出紙，`EXIT` 可回到可操作的 MAINAPP。
