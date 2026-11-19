"""Download and install the Windows ARM compiler and glibc 2.15 sysroot."""

import logging
from pathlib import Path
import shutil
from urllib.request import urlopen

from tqdm import tqdm

from extract import extract_compiler, extract_sysroot


log = logging.getLogger(__name__)
HERE = Path(__file__).resolve().parent
GCC = "gcc-linaro-7.5.0-2019.12-i686-mingw32_arm-linux-gnueabi"
GCC_ARCHIVE = HERE / f"{GCC}.tar.xz"
GCC_URL = ("https://developer.arm.com/-/cdn-downloads/permalink/"
           f"legacy-linaro-gnu-toolchains/7.5-2019.12/{GCC_ARCHIVE.name}")
SYSROOT_ARCHIVE = HERE / "arm-2012.03-57-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2"
SYSROOT_URL = ("https://people.freebsd.org/~danfe/arm-none-linux-gnueabi-gcc-files/"
               + SYSROOT_ARCHIVE.name)


def download(url: str, archive: Path) -> None:
    if archive.is_file():
        return
    part = archive.with_name(archive.name + ".part")
    print(f"Download {archive.name}", flush=True)
    with urlopen(url, timeout=60) as response, part.open("wb") as output:
        size = int(response.headers.get("Content-Length", 0))
        with tqdm.wrapattr(output, "write", total=size, dynamic_ncols=True,
                           unit="B", unit_scale=True, unit_divisor=1024) as progress:
            shutil.copyfileobj(response, progress)
    part.replace(archive)


def main() -> None:
    log.debug("Prepare Windows ARM toolchain")
    download(GCC_URL, GCC_ARCHIVE)
    download(SYSROOT_URL, SYSROOT_ARCHIVE)
    compiler_dir = HERE / "compiler" / GCC
    if not (compiler_dir / "bin/arm-linux-gnueabi-gcc.exe").is_file():
        print(f"Extract {GCC_ARCHIVE.name}", flush=True)
        extract_compiler(GCC_ARCHIVE, GCC, compiler_dir)
    if not (HERE / "sysroot/usr/lib/crt1.o").is_file():
        print(f"Extract {SYSROOT_ARCHIVE.name}", flush=True)
        extract_sysroot(SYSROOT_ARCHIVE, HERE / "sysroot")
    print("[toolchain] ✅ Toolchain ready")


if __name__ == "__main__":
    main()
