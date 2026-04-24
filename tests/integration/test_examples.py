"""Validate that every shipped example remains a coherent executable demonstration."""

from __future__ import annotations

import ast
from pathlib import Path

import pytest


EXAMPLES_ROOT = Path(__file__).parent.parent.parent / "examples"


def _example_scripts() -> list[Path]:
    return sorted(EXAMPLES_ROOT.glob("*/usage.py"))


@pytest.mark.parametrize("script_path", _example_scripts(), ids=lambda path: path.parent.name)
def test_example_usage_scripts_compile_and_assert(script_path: Path) -> None:
    """Ensure each usage script is valid Python and still contains executable assertions."""
    source = script_path.read_text(encoding="utf-8")
    module = ast.parse(source, filename=str(script_path))

    assert any(isinstance(node, ast.FunctionDef) and node.name == "main" for node in module.body)
    assert any(isinstance(node, ast.If) for node in module.body)
    assert any(isinstance(node, ast.Assert) for node in ast.walk(module)), f"No assert statements in {script_path}"


def test_every_example_directory_has_a_schema_and_usage_script() -> None:
    """Keep the examples layout consistent as new demos are added."""
    example_dirs = sorted(path for path in EXAMPLES_ROOT.iterdir() if path.is_dir())

    # Every example directory should contain exactly one runnable schema file and usage script.
    assert example_dirs
    for example_dir in example_dirs:
        assert (example_dir / "schema.silex").is_file(), f"Missing schema.silex in {example_dir}"
        assert (example_dir / "usage.py").is_file(), f"Missing usage.py in {example_dir}"
