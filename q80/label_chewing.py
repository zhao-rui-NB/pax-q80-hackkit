"""Build a Zhuyin-only libchewing for the Q80 label editor."""

import logging
from pathlib import Path, PurePosixPath
import subprocess
import tarfile

from q80.config import ROOT, load
from q80.log import success


log = logging.getLogger(__name__)
APP = ROOT / "device/apps/label_maker"
ARCHIVE = ROOT / "vendor/libchewing/libchewing-v0.5.1.tar.gz"
WORK = ROOT / "build/label_maker/zhuyin-only"
SOURCE = WORK / "libchewing-0.5.1"
OBJECTS = WORK / "objects"
FILES = (
    "src/compat.c", "src/chewingio.c", "src/chewingutil.c", "src/choice.c",
    "src/dict.c", "src/mod_aux.c", "src/tree.c", "src/userphrase.c",
    "src/bopomofo.c", "src/hash.c", "src/userphrase-hash.c",
    "src/common/chewing-utf8-util.c", "src/common/key2pho.c",
    "src/porting_layer/src/plat_mmap_posix.c",
    "src/porting_layer/src/plat_mmap_windows.c",
    "src/porting_layer/src/plat_path.c",
    "src/porting_layer/src/rpl_malloc.c",
)
CONFIG = """#define HAVE_SYS_TYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRTOK_R 1
#define HAVE_ASPRINTF 1
#define HAVE_UNISTD_H 1
#define HAVE_MALLOC 1
"""


def _replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError("Unexpected libchewing source version")
    return text.replace(old, new)


def _patch() -> None:
    path = SOURCE / "src/chewingio.c"
    text = path.read_text(encoding="utf-8")
    text = _replace_once(text, '#include "pinyin-private.h"\n', "")
    text = _replace_once(text, '''const char *const PINYIN_FILES[] = {
    PINYIN_TAB_NAME,
    NULL,
};

''', "")
    text = _replace_once(text, '''    ret = find_path_by_files(search_path, PINYIN_FILES, path, sizeof(path));
    if (ret) {
        LOG_ERROR("find_path_by_files returns %d", ret);
        goto error;
    }

    ret = InitPinyin(ctx->data, path);
    if (!ret) {
        LOG_ERROR("InitPinyin returns %d", ret);
        goto error;
    }

''', "")
    text = _replace_once(text, "            TerminatePinyin(ctx->data);\n", "")
    text = _replace_once(text, "if (kbtype < KB_TYPE_NUM && kbtype >= 0)",
                         "if (kbtype < KB_HANYU_PINYIN && kbtype >= 0)")
    path.write_text(text, encoding="utf-8", newline="\n")

    path = SOURCE / "src/bopomofo.c"
    text = path.read_text(encoding="utf-8")
    text = _replace_once(text, '#include "pinyin-private.h"\n', "")
    start = text.index("static int IsPinYinEndKey(int key)")
    end = text.index("/* key: ascii code of input, including space */", start)
    text = text[:start] + text[end:]
    text = _replace_once(text, '''    case KB_HANYU_PINYIN:
    case KB_THL_PINYIN:
    case KB_MPS2_PINYIN:
        return PinYinInput(pgdata, key);
        break;
''', "")
    path.write_text(text, encoding="utf-8", newline="\n")


def build() -> Path:
    """Unpack the vendored source, remove Pinyin support, and cross-compile."""
    SOURCE.mkdir(parents=True, exist_ok=True)
    with tarfile.open(ARCHIVE, "r:gz") as archive:
        for member in archive:
            relative = PurePosixPath(member.name)
            if (relative.is_absolute() or ".." in relative.parts or
                    relative.parts[0] != SOURCE.name):
                raise RuntimeError("Unexpected source archive path")
            target = WORK.joinpath(*relative.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                source = archive.extractfile(member)
                if source is None:
                    raise RuntimeError("Source archive member missing")
                target.write_bytes(source.read())
            else:
                raise RuntimeError("Unexpected source archive member type")
    _patch()
    config_dir = WORK / "include"
    config_dir.mkdir(exist_ok=True)
    (config_dir / "config.h").write_text(CONFIG, encoding="ascii")
    OBJECTS.mkdir(exist_ok=True)
    compiler = load().compiler
    flags = [
        "-std=gnu99", "-O2", "-fPIC", "-march=armv6", "-marm",
        "-mfloat-abi=soft", "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables", "-DUNDER_POSIX", "-DPIC",
        "-D_GNU_SOURCE", "-DHAVE_CONFIG_H=1",
        '-DCHEWING_DATADIR="/data/app/label_maker/data"',
        *(f"-I{directory}" for directory in (
            SOURCE / "include", SOURCE / "include/internal", SOURCE / "src",
            SOURCE / "src/common", SOURCE / "src/porting_layer/include", config_dir,
        )),
    ]
    objects = []
    for name in FILES:
        output = OBJECTS / (name.replace("/", "_") + ".o")
        subprocess.run([compiler, *flags, "-c", str(SOURCE / name), "-o", str(output)],
                       check=True)
        objects.append(str(output))
    output = OBJECTS / "libchewing.so"
    subprocess.run([compiler, "-shared", "-nostdlib", "-Wl,--hash-style=sysv",
                    "-Wl,--build-id=none", "-o", str(output), *objects, "-lgcc"],
                   check=True)
    if output.read_bytes()[:6] != b"\x7fELF\x01\x01":
        raise RuntimeError("Cross-compiler produced an unexpected binary")
    success(log, "Zhuyin library built: %s", output)
    return output
