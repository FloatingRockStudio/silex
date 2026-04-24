# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Build the Silex documentation site with Doxygen."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


def build_docs(output_dir: Path) -> Path:
    """Render the checked-in docs into a static HTML site."""
    project_root = Path(__file__).resolve().parent.parent
    build_root = project_root / ".docs_build"
    doxyfile_template = project_root / "docs" / "Doxyfile.in"
    generated_doxyfile = build_root / "Doxyfile"

    if not shutil.which("doxygen"):
        raise RuntimeError("doxygen was not found on PATH")

    build_root.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    doxyfile_contents = doxyfile_template.read_text(encoding="utf-8")
    replacements = {
        "@SOURCE_PATH@": project_root.as_posix(),
        "@BUILD_PATH@": build_root.as_posix(),
        "@OUTPUT_PATH@": output_dir.as_posix(),
    }
    for token, value in replacements.items():
        doxyfile_contents = doxyfile_contents.replace(token, value)

    generated_doxyfile.write_text(doxyfile_contents, encoding="utf-8")
    subprocess.run(["doxygen", str(generated_doxyfile)], check=True)
    return output_dir


def main() -> int:
    """Parse arguments and build the site."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", default="site", help="Directory to receive the generated HTML site.")
    args = parser.parse_args()

    output_dir = Path(args.output_dir).resolve()
    build_docs(output_dir)
    print(output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
