"""App metadata shared by development deployment and AIP packing."""

from dataclasses import dataclass
import logging
from pathlib import Path
import re
import tomllib

from q80.config import ROOT
from q80.log import success


log = logging.getLogger(__name__)


@dataclass(frozen=True)
class App:
    folder: Path
    app_id: str
    version: str
    name: str
    binary: str
    libraries: tuple[str, ...] = ()

    @property
    def output(self) -> Path:
        return ROOT / "build" / self.app_id / self.binary

    @property
    def appinfo(self) -> bytes:
        return (
            f"[app]\nid={self.app_id}\nname={self.name}\n"
            f"bin={self.binary}\nfile_type=dll\nversion={self.version}\n"
        ).encode("utf-8")


def load(name: str) -> App:
    folder = ROOT / "device" / "apps" / name
    path = folder / "app.toml"
    if not path.is_file():
        raise FileNotFoundError(path)
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    app_id = data.get("id", name)
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,31}", app_id) or app_id == "MAINAPP":
        raise ValueError("DLL app ID must be 1-32 letters, digits or underscores; not MAINAPP")
    binary = data.get("binary", f"lib{app_id}.so")
    if Path(binary).name != binary or not binary.endswith(".so"):
        raise ValueError("binary must be a .so filename")
    return App(folder, app_id, str(data.get("version", "0.1.0")),
               str(data.get("name", app_id)), binary,
               tuple(data.get("libraries", [])))


def create(name: str) -> Path:
    if not re.fullmatch(r"[a-z][a-z0-9_]{0,31}", name):
        raise ValueError("App name must use lowercase letters, digits and underscores")
    folder = ROOT / "device" / "apps" / name
    folder.mkdir(parents=True, exist_ok=False)
    (folder / "app.toml").write_text(
        f'id = "{name}"\nname = "{name}"\nversion = "0.1.0"\n', encoding="utf-8"
    )
    (folder / "main.c").write_text(
        "/* Minimal Q80 DLL app. The runtime calls app_main. */\n"
        "int app_main(int argc, char **argv) {\n"
        "    (void)argc; (void)argv;\n"
        "    return 0;\n"
        "}\n", encoding="utf-8"
    )
    success(log, "App created: %s", folder)
    return folder
