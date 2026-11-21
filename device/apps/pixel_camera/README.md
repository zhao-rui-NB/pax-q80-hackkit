# Q80 Pixel Camera 像素相機

`pixel_camera` 使用 Q80 後鏡頭即時預覽。按 `SNAP` 擷取畫面，直接縮至印表機完整解析度 384×288。螢幕會顯示與出紙相同的黑白預覽；按 `PRINT` 才會列印。`MODE` 在兩種模式間切換並回到即時畫面，切換後要重新按 `SNAP`。`RETAKE` 回到即時鏡頭，`EXIT` 或實體 Cancel 鍵返回 MAINAPP。啟動與拍照都不會出紙。

| 模式 | 轉圖方式 |
| --- | --- |
| `BOLD`（預設） | 3×3 平滑後依整張照片亮度分布找黑白分界，輸出連續輪廓與實面 |
| `SHADE` | 將每個 3×3 點小格的平均亮度換成 0–8 個相連黑點，以黑點密度表示簡單灰階；自動調亮以控制整張圖的黑點比例 |

最新一張黑白預覽儲存於裝置的 `/data/app/pixel_camera/latest_pixel.png`，再次拍照會覆蓋；不儲存原始彩色照片，也不自動上傳。

```powershell
python -m q80 app build pixel_camera
python -m q80 app deploy pixel_camera
```

影像使用已在 `printer` App 驗證的 `OsPrnPutImage` 封包格式，將 384×288 點分為兩段各 144 行，使用列印濃度 4。原廠手冊的熱感印表機上限是每行 384 點，也就是 48 bytes；`OsPrnSetSize()` 的 600 點上限只適用虛擬印表機，`OsPrnGetDotLine()` 回傳的是已印行數。早期 2×2 放大使細節不足，Floyd–Steinberg 抖色使紙上雜點過多，逐點集中網點仍不易辨識主體；Atkinson 在本機也不如 `BOLD` 清楚。`SHADE` 改以小格平均亮度決定相連黑點數，實機的螢幕預覽與紙張結果均比先前模式清楚。使用者仍希望依參考圖優化點形與排列。

參考 [Apple 的網點說明](https://developer.apple.com/documentation/coreimage/cifilter-swift.class/dotscreen%28%29) 與 [IEEE 對集中網點的研究](https://ieeexplore.ieee.org/abstract/document/1215229)：當印表機無法穩定呈現孤立單點時，集中網點較合適。[Adafruit 熱感印表機實測](https://learn.adafruit.com/mini-thermal-receipt-printer/hacking)也指出密集黑點可能讓照片模糊。這些資料不是 Q80 的實測結果，因此仍以本機紙張測試判斷。
