"""Prepare the Windows ARM build tools."""

import argparse
import hashlib
import logging
from pathlib import Path
import shutil
from urllib.request import urlopen

from tqdm import tqdm

from toolchain.extract import extract_compiler, extract_sysroot

from q80.build import _check_arm
from q80.log import configure, report_error, success


log = logging.getLogger(__name__)
HERE = Path(__file__).resolve().parent.parent / "toolchain"
GCC = "gcc-linaro-7.5.0-2019.12-i686-mingw32_arm-linux-gnueabi"
GCC_ARCHIVE = HERE / f"{GCC}.tar.xz"
GCC_URL = ("https://developer.arm.com/-/cdn-downloads/permalink/"
           f"legacy-linaro-gnu-toolchains/7.5-2019.12/{GCC_ARCHIVE.name}")
SYSROOT_ARCHIVE = HERE / "arm-2012.03-57-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2"
SYSROOT_URL = ("https://people.freebsd.org/~danfe/arm-none-linux-gnueabi-gcc-files/"
               + SYSROOT_ARCHIVE.name)


GCC_SHA256 = "8a7624121d9e75ff2a2ac089de5746d663986b75e71b8bf41a458f95413877c2"
SYSROOT_SHA256 = "3dad8547de7195a8afa5f449a04270bd290603e6ec5e50bebb325d5fe1a97251"
DEVICE_LIBRARIES = ("libosal.so", "libxui.so")


def verify_archive(path: Path, expected: str) -> None:
    with path.open("rb") as source:
        actual = hashlib.file_digest(source, "sha256").hexdigest()
    if actual != expected:
        raise ValueError(f"Archive checksum mismatch: {path.name}")


def download(url: str, archive: Path, expected: str) -> None:
    if archive.is_file():
        verify_archive(archive, expected)
        return
    part = archive.with_name(archive.name + ".part")
    log.info("Download %s", archive.name)
    try:
        with urlopen(url, timeout=60) as response, part.open("wb") as output:
            size = int(response.headers.get("Content-Length", 0))
            with tqdm.wrapattr(output, "write", total=size, dynamic_ncols=True,
                               unit="B", unit_scale=True, unit_divisor=1024) as progress:
                shutil.copyfileobj(response, progress)
        verify_archive(part, expected)
        part.replace(archive)
    finally:
        part.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    configure()
    try:
        for name in DEVICE_LIBRARIES:
            library = HERE / "device-libs" / name
            if not library.is_file():
                raise FileNotFoundError(f"Missing repository file: {library}")
            _check_arm(library)
        compiler_dir = HERE / "compiler" / GCC
        if not (compiler_dir / "bin/arm-linux-gnueabi-gcc.exe").is_file():
            download(GCC_URL, GCC_ARCHIVE, GCC_SHA256)
            log.info("Extract %s", GCC_ARCHIVE.name)
            extract_compiler(GCC_ARCHIVE, GCC, compiler_dir)
        if not (HERE / "sysroot/usr/lib/crt1.o").is_file():
            download(SYSROOT_URL, SYSROOT_ARCHIVE, SYSROOT_SHA256)
            log.info("Extract %s", SYSROOT_ARCHIVE.name)
            extract_sysroot(SYSROOT_ARCHIVE, HERE / "sysroot")
    except (OSError, RuntimeError, ValueError) as exc:
        report_error(exc)
        return 1
    success(log, "Toolchain ready")
    return 0
