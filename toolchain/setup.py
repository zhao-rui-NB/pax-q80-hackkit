"""Run the Q80 build environment setup from the toolchain directory."""

from pathlib import Path
import sys


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from q80.toolchain_setup import main

    raise SystemExit(main())
