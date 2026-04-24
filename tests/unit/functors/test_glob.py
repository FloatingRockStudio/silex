"""
Tests for the glob functor.
"""

import pytest
import tempfile
import os
from pathlib import Path

pytest.importorskip("silex.functors", reason="silex C++ bindings not available")

from silex import Box

from silex.functors import GlobFunctor
from silex import FunctorOutput, FunctorContext


@pytest.fixture
def context():
    """Create a basic context for testing."""
    return FunctorContext(context=Box({}), parent="/some/path", segment="test", variables=Box())


@pytest.fixture
def temp_directory():
    """Create a temporary directory with test files."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test files and directories
        (temp_path / "FR00069_DGS").mkdir()
        (temp_path / "FR00089_GH2").mkdir()
        (temp_path / "TESTPIPE").mkdir()
        (temp_path / "test_file.txt").touch()
        (temp_path / "another_file.py").touch()

        yield temp_path


def _outputs_from_strings(output_names):
    """Helper to create FunctorOutput list from strings."""
    if isinstance(output_names, str):
        output_names = [output_names]
    return [FunctorOutput(name=name, options=[]) for name in output_names]


class TestGlobFunctor:
    """Test the Glob functor."""

    @pytest.fixture
    def functor(self):
        return GlobFunctor()

    def test_write_basic_match(self, functor, temp_directory):
        """Test basic write functionality with a matching pattern."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["FR*_DGS"], context)
        assert result.success
        assert result.output == "FR00069_DGS"
        assert "Found 1 matches" in result.message

    def test_write_multiple_matches(self, functor, temp_directory):
        """Test write with multiple matches returns first."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["FR*"], context)
        assert result.success
        # Should return one of the FR directories (first alphabetically)
        assert result.output in ["FR00069_DGS", "FR00089_GH2"]
        assert "Found 2 matches" in result.message
        assert result.matches == ["FR00069_DGS", "FR00089_GH2"]

    def test_write_uses_ordered_patterns_and_preserves_all_candidates(self, functor, temp_directory):
        """Test ordered write patterns keep priority while preserving all candidate matches."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["FR*_DGS", "*", ""], context)

        assert result.success
        assert result.output == "FR00069_DGS"
        assert result.matches == [
            "FR00069_DGS",
            "FR00089_GH2",
            "TESTPIPE",
            "another_file.py",
            "test_file.txt",
        ]

    def test_write_uses_truthy_final_input_as_default(self, functor, temp_directory):
        """Test the final input acts as a fallback when no patterns match."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["NO_MATCH*", "fallback_dir"], context)

        assert result.success
        assert result.output == "fallback_dir"
        assert result.matches == []

    def test_write_match_is_case_insensitive(self, functor, temp_directory):
        """Test write matches existing entries regardless of case."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["FR*_dgs"], context)
        assert result.success
        assert result.output == "FR00069_DGS"

    def test_write_no_matches(self, functor, temp_directory):
        """Test write with no matching pattern."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["NONEXISTENT*"], context)
        assert not result.success
        assert result.output == ""
        assert "No matches found" in result.message

    def test_write_file_pattern(self, functor, temp_directory):
        """Test write with file pattern matching."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())

        result = functor.format(["*.txt"], context)
        assert result.success
        assert result.output == "test_file.txt"

    def test_write_empty_input(self, functor, context):
        """Test write with empty input."""
        result = functor.format([], context)
        assert not result.success
        assert "No glob pattern provided" in result.message

    def test_write_invalid_pattern(self, functor, context):
        """Test write with invalid pattern/directory."""
        # Use a non-existent directory
        bad_context = FunctorContext(context=Box({}), parent="/nonexistent/path", segment="test", variables=Box())

        result = functor.format(["*"], bad_context)
        assert not result.success
        assert result.output == ""

    def test_read_basic_match(self, functor, temp_directory):
        """Test basic read functionality with matching pattern."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())
        outputs = _outputs_from_strings("result")

        result = functor.parse(["FR*_DGS"], outputs, context)
        assert result.success
        assert result.outputs["result"].value == "FR00069_DGS"
        assert not result.outputs["result"].is_ambiguous

    def test_read_multiple_matches_ambiguous(self, functor, temp_directory):
        """Test read with multiple matches marks as ambiguous."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())
        outputs = _outputs_from_strings("result")

        result = functor.parse(["FR*"], outputs, context)
        assert result.success
        assert result.outputs["result"].value in ["FR00069_DGS", "FR00089_GH2"]
        assert result.outputs["result"].is_ambiguous  # Multiple matches

    def test_read_multiple_outputs(self, functor, temp_directory):
        """Test read with multiple outputs."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())
        outputs = _outputs_from_strings(["result1", "result2"])

        result = functor.parse(["FR*_DGS"], outputs, context)
        assert result.success
        assert result.outputs["result1"].value == "FR00069_DGS"
        assert result.outputs["result2"].value == "FR00069_DGS"

    def test_read_no_matches(self, functor, temp_directory):
        """Test read with no matching pattern."""
        context = FunctorContext(context=Box({}), parent=str(temp_directory), segment="test", variables=Box())
        outputs = _outputs_from_strings("result")

        result = functor.parse(["NONEXISTENT*"], outputs, context)
        assert not result.success
        assert "No matches found" in result.message

    def test_read_empty_input(self, functor, context):
        """Test read with empty input."""
        outputs = _outputs_from_strings("result")
        result = functor.parse([], outputs, context)
        assert not result.success
        assert "No glob pattern provided" in result.message
