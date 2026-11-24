# Label Maker

可用大千注音、英文或數字編輯一張標籤。點白色標籤直接輸入，按 `DONE` 回主畫面預覽，再點 `PRINT` 印一張。預覽從紙張頂端開始，較長的標籤可上下滑動查看。

熱感紙寬為 384 點。點 `DEG` 切換直印和橫印，按鈕為藍色時表示橫印。直印按紙寬換行，並依行數將文字區塊置中。橫印將文字旋轉 90 度，沿走紙方向逐行置中。兩種方向都依內容決定用紙長度，每張最多 8192 點。`-` 和 `+` 可調整字級，或點 `SIZE` 用實體數字鍵輸入 14–500，再按綠色確認鍵套用。超過 120 點時會放大字型點陣，橫印時手動換行的行數受紙寬及字級限制。

預覽頁可按實體綠鍵列印，紅鍵退出，橘色清除鍵每按一次送出 96 點空白紙。編輯頁可用實體 2、8、4、6 移動游標，0 是空白鍵，橘色清除鍵對應 `DEL`，綠鍵對應 `DONE`。

當文字超過紙寬時，預覽仍以指定字級顯示紙寬內的部分，並標示 `CROP`。列印使用相同的裁切結果。

編輯器一次顯示排版中的一行，放大文字並將正在輸入的注音顯示在下方。狀態列顯示目前行號和總行數，可用 `UP` 和 `DOWN` 切換行，或點文字及用 `<` 和 `>` 移動游標。`NEW LINE` 在游標處換行，`DEL` 可刪除前一個字或合併兩行。注音音節尚未打完時，先完成音節再移動或換行。

## 執行檔案

- `main.c`、`ime.c`、`ime.h`、`keycaps.h`：XUI 畫面與注音輸入。
- `runtime/libchewing.so`：移除拼音來源與初始化的 libchewing 0.5.1。進入編輯器時，`ime.c` 用 `dlopen` 載入它，再用 `dlsym` 取得注音函式。
- `runtime/data/dictionary.dat`、`index_tree.dat`：中文字與候選字資料。
- `runtime/data/symbols.dat`、`swkb.dat`：目前函式庫建立輸入環境時仍會載入，不能只刪檔。
- `runtime/NotoSansTC.ttf`：顯示機器內建字型缺少的中文字；授權見 `LICENSE-NOTO.txt`。

裝置上的 `userphrase.dat` 由輸入引擎建立，用來保存學習詞。上游原始碼與 LGPL 授權放在專案的 `vendor/libchewing/`，不會部署到裝置。重建注音函式庫：

```powershell
python -c "from q80.log import configure; from q80.label_chewing import build; configure(); build()"
Copy-Item build/label_maker/zhuyin-only/objects/libchewing.so device/apps/label_maker/runtime/libchewing.so
```

已在 Q80 隔離測試及安裝後測試：沒有 `pinyin.tab` 時，注音候選字仍可輸出「爸爸」。預覽和列印共用黑白點陣圖，長標籤由 OSAL 分段沿紙卷印出。
