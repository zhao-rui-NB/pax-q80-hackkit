"""Extract the Windows compiler and ARM sysroot from their archives."""

import logging
from pathlib import Path, PurePosixPath
import shutil
import tarfile

from tqdm import tqdm


log = logging.getLogger(__name__)


def _copy(package: tarfile.TarFile, member: tarfile.TarInfo, output: Path) -> None:
    source = package.extractfile(member)  # Materialize Unix links as files on Windows.
    if source is None:
        raise ValueError(f"Cannot read archive entry: {member.name}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with source, output.open("wb") as destination:
        shutil.copyfileobj(source, destination)


def extract_compiler(archive: Path, package_name: str, output: Path) -> None:
    log.debug("Extract compiler: %s", archive)
    with tarfile.open(archive) as package:
        files = []
        for member in package:
            parts = PurePosixPath(member.name).parts
            if parts[:1] != (package_name,) or ".." in parts:
                continue
            if len(parts) > 1 and (member.isfile() or member.issym() or member.islnk()):
                files.append((member, output.joinpath(*parts[1:])))
        for member, target in tqdm(files, unit="file", dynamic_ncols=True):
            _copy(package, member, target)


def extract_sysroot(archive: Path, output: Path) -> None:
    log.debug("Extract ARM sysroot: %s", archive)
    prefix = ("arm-2012.03", "arm-none-linux-gnueabi", "libc")
    excluded = (("usr", "lib", "locale"), ("usr", "lib", "gconv"),
                ("usr", "lib", "bin"), ("usr", "lib", "audit"))
    with tarfile.open(archive) as package:
        files = []
        for member in package:
            parts = PurePosixPath(member.name).parts
            if parts[:3] != prefix or ".." in parts:
                continue
            name = parts[3:]
            if not name or name[:3] in excluded:
                continue
            if name[0] != "lib" and name[:2] not in (("usr", "include"), ("usr", "lib")):
                continue
            if member.isfile() or member.issym() or member.islnk():
                files.append((member, output.joinpath(*name)))
        for member, target in tqdm(files, unit="file", dynamic_ncols=True):
            _copy(package, member, target)
