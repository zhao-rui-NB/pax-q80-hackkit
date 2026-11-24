# Label Maker

第一版可用大千注音、英文或數字編輯一張標籤。點 `EDIT TEXT` 輸入，按 `DONE` 回主畫面預覽，再點 `PRINT LABEL` 印一張。版型為 384×144 點，最多三行置中，會依文字寬度自動換行。空白或超出版型的文字無法列印。

## 執行檔案

- `main.c`、`ime.c`、`ime.h`、`keycaps.h`：XUI 畫面與注音輸入。
- `runtime/libchewing.so`：移除拼音來源與初始化的 libchewing 0.5.1；不需要 `pinyin.tab`。
- `runtime/data/dictionary.dat`、`index_tree.dat`：中文字與候選字資料。
- `runtime/data/symbols.dat`、`swkb.dat`：目前函式庫建立輸入環境時仍會載入，不能只刪檔。
- `runtime/NotoSansTC.ttf`：顯示機器內建字型缺少的中文字；授權見 `LICENSE-NOTO.txt`。

裝置上的 `userphrase.dat` 由輸入引擎建立，用來保存學習詞。上游原始碼與 LGPL 授權放在專案的 `vendor/libchewing/`，不會部署到裝置。重建注音函式庫：

```powershell
python -c "from q80.log import configure; from q80.label_chewing import build; configure(); build()"
Copy-Item build/label_maker/zhuyin-only/objects/libchewing.so device/apps/label_maker/runtime/libchewing.so
```

已在 Q80 隔離測試及安裝後測試：沒有 `pinyin.tab` 時，注音候選字仍可輸出「爸爸」。標籤列印使用 Q80 的 OSAL 印表機，直接擷取預覽畫布轉成黑白點陣圖。
