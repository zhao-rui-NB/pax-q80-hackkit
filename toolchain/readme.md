# Windows ARM 編譯環境

先自行安裝 Python 套件 `tqdm`：

```powershell
python -m pip install tqdm
```

再於專案根目錄執行：

```powershell
python toolchain/setup.py
```

腳本會下載兩份壓縮檔到 `toolchain/`，用 `tqdm` 顯示下載與解壓進度，並呼叫 `extract.py` 解出編譯需要的檔案。

| 來源 | 解出位置 | 用途 |
| --- | --- | --- |
| [Arm：Windows 版 Linaro GCC 7.5.0](https://developer.arm.com/-/cdn-downloads/permalink/legacy-linaro-gnu-toolchains/7.5-2019.12/gcc-linaro-7.5.0-2019.12-i686-mingw32_arm-linux-gnueabi.tar.xz)（[下載頁](https://developer.arm.com/Downloads/-/Legacy%20Linaro%20GNU%20Toolchains)） | `toolchain/compiler/` | 在 Windows 執行的 ARM 編譯器 |
| [Sourcery CodeBench 2012.03 壓縮檔](https://people.freebsd.org/~danfe/arm-none-linux-gnueabi-gcc-files/arm-2012.03-57-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2)（[鏡像目錄](https://people.freebsd.org/~danfe/arm-none-linux-gnueabi-gcc-files/)） | `toolchain/sysroot/` | ARM glibc 2.15 標頭、啟動檔與函式庫 |

sysroot 只取原套件 `arm-2012.03/arm-none-linux-gnueabi/libc/` 下的 `lib/`、`usr/include/` 和 `usr/lib/`，略過 locale、gconv 與工具檔。壓縮檔會保留；已下載或已解出的部分不會重做。產物已被 Git 忽略。

Q80 的 XUI／OSAL C API 宣告與使用限制見 [include/README.md](include/README.md)。

若 App 要直接連結裝置庫，先依 [linked_hello 範例](../device/apps/linked_hello/README.md) 將 Q80 的 `libosal.so`、`libxui.so` 複製到 `toolchain/device-libs/`。這些本機副本只供編譯連結，不會打包進 App，且已被 Git 忽略。
