# 在 Windows 編譯 `main()` 程式

使用 Windows 版 Linaro GCC 7.5.0 與 ARM glibc 2.15 sysroot。這是一般 C 執行檔，可使用 `main()`、標準標頭與 libc；既有的 `app` 命令則處理 `app_main` DLL。

## 準備

先安裝 `tqdm`，再從專案根目錄執行：

```powershell
python -m pip install tqdm
python toolchain/setup.py
```

腳本會自動下載 [Windows 版 Linaro GCC 7.5.0](https://developer.arm.com/-/cdn-downloads/permalink/legacy-linaro-gnu-toolchains/7.5-2019.12/gcc-linaro-7.5.0-2019.12-i686-mingw32_arm-linux-gnueabi.tar.xz) 和 [Sourcery CodeBench 2012.03](https://people.freebsd.org/~danfe/arm-none-linux-gnueabi-gcc-files/arm-2012.03-57-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2)，顯示下載與解壓進度，將壓縮檔保留在 `toolchain/`，解出 `toolchain/compiler/` 與 `toolchain/sysroot/`。編譯命令會優先使用這套 Windows GCC。來源與目錄用途見 [toolchain 說明](../toolchain/readme.md)。

## 編譯與執行

```powershell
python -m q80 app-exe build examples/main_hello.c build/main_hello
python -m q80 xcb push build/main_hello /data/tmp/q80_main_hello
python -m q80 shell /lib/ld-linux.so.3 /data/tmp/q80_main_hello
```

範例在 Q80 輸出 `Q80_MAIN_OK`。目前一次編譯一個 C 原始檔，輸出路徑由最後一個參數指定。

編譯使用 `--sysroot=toolchain/sysroot`、`-std=gnu99 -march=armv6 -marm -mfloat-abi=soft -Wl,--hash-style=sysv -Wl,--build-id=none`。GCC 從 sysroot 取標頭、啟動檔與 libc，再由 C runtime 呼叫 `main()`。GCC 7.5.0 編出的範例為 ARM ELF32、EABI5、soft-float，interpreter 為 `/lib/ld-linux.so.3`，只要求 `GLIBC_2.4`；加入其他函式庫時須重新檢查產物要求的版本。

裝置原廠 `/lib/libc.so.6` 的最高公開符號版本為 `GLIBC_2.15`。sysroot 的 libc 也是 2.15，但並非裝置上同一份 binary。GCC 版本可以不同；執行檔實際引用的函數版本仍須在裝置上存在。

直接執行未簽章 ELF 曾得到 `Invalid argument`，裝置 `dmesg` 同時出現 `signature FAILED`。上面的實測方式是明確呼叫裝置原廠 loader。
