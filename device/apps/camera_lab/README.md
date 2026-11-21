# Q80 Camera Lab

`camera_lab` 使用 XUI 的相機視窗顯示後鏡頭即時預覽。`CAPTURE` 拍照並顯示照片，將最新一張儲存為 `/data/app/camera_lab/latest.png`；`LIVE` 返回即時畫面；`EXIT` 或實體 Cancel 鍵回到 MAINAPP。每次拍照覆蓋上一張，不會自動上傳。

```powershell
python -m q80 app build camera_lab
python -m q80 app deploy camera_lab
```

此機 `libxui.so` 沒有匯出手冊中的 `XuiCameraSetStat()`，所以 App 使用畫布及獨立按鈕，不呼叫此函式。實機的 `640×480` 解析度代碼是 `1`；代碼 `0` 會使 `XuiCreateCamera()` 失敗。已在 Q80 確認即時預覽、拍照、照片檢視、返回即時預覽，以及從 MAINAPP 啟動並正常退出。
