# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""
Unit tests for the GlobTag functor with output options.
"""

import pytest
import tempfile
from pathlib import Path

pytest.importorskip("silex.functors", reason="silex C++ bindings not available")

from silex.functors import GlobTagFunctor
from silex import Box, FunctorContext, FunctorOutput


class TestGlobTagFunctorOptions:
    """Test GlobTag functor with different output options."""

    @pytest.fixture
    def test_context(self):
        """Create a test context with temporary files."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Create test files
            (temp_path / "test_v001.ma").write_text("version 1")
            (temp_path / "test_v002.ma").write_text("version 2")
            (temp_path / "test_v010.ma").write_text("version 10")

            # Create tag files
            (temp_path / ".test_v001.published").write_text("")
            (temp_path / ".test_v002.deprecated").write_text("")
            (temp_path / ".test_v010.wip").write_text("")

            context = FunctorContext(
                parent=str(temp_path),
                variables=Box({
                    "tags": {
                        "published": {"alias": ["published"], "explicit": True},
                        "latest": {"alias": ["latest"], "explicit": False},
                        "deprecated": {"alias": ["deprecated"], "explicit": True},
                        "wip": {"alias": ["wip", "work"], "explicit": False},
                    }
                }),
            )

            yield context

    def test_output_option_version(self, test_context):
        """Test output with version option."""
        functor = GlobTagFunctor()

        # Create output with version option
        outputs = [FunctorOutput(name="result", options=["version"])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert "result" in result.outputs
        assert result.outputs["result"].value == 10  # Highest version

    def test_output_option_name(self, test_context):
        """Test output with name option."""
        functor = GlobTagFunctor()

        # Create output with name option
        outputs = [FunctorOutput(name="result", options=["name"])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert "result" in result.outputs
        assert result.outputs["result"].value == "test_v010.ma"

    def test_output_option_tags(self, test_context):
        """Test output with tags option."""
        functor = GlobTagFunctor()

        # Create output with tags option
        outputs = [FunctorOutput(name="result", options=["tags"])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert "result" in result.outputs
        assert isinstance(result.outputs["result"].value, list)

    def test_output_option_tag_single(self, test_context):
        """Test output with tag option (single tag)."""
        functor = GlobTagFunctor()

        # Create output with tag option
        outputs = [FunctorOutput(name="result", options=["tag"])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert "result" in result.outputs
        # Should return first tag or empty string
        tag_value = result.outputs["result"].value
        assert isinstance(tag_value, str)

    def test_output_inferred_from_name_version(self, test_context):
        """Test output type inferred from name when no options."""
        functor = GlobTagFunctor()

        # Create outputs with names that should be inferred
        outputs = [
            FunctorOutput(name="version", options=[]),
            FunctorOutput(name="ver", options=[]),
            FunctorOutput(name="v", options=[]),
        ]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        for output_name in ["version", "ver", "v"]:
            assert output_name in result.outputs
            assert result.outputs[output_name].value == 10

    def test_output_inferred_from_name_tags(self, test_context):
        """Test output type inferred from name for tags."""
        functor = GlobTagFunctor()

        outputs = [FunctorOutput(name="tags", options=[]), FunctorOutput(name="tag_list", options=[])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        for output_name in ["tags", "tag_list"]:
            assert output_name in result.outputs
            assert isinstance(result.outputs[output_name].value, list)

    def test_output_inferred_from_name_single_tag(self, test_context):
        """Test output type inferred from name for single tag."""
        functor = GlobTagFunctor()

        outputs = [FunctorOutput(name="tag", options=[]), FunctorOutput(name="single_tag", options=[])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        for output_name in ["tag", "single_tag"]:
            assert output_name in result.outputs
            assert isinstance(result.outputs[output_name].value, str)

    def test_output_inferred_from_name_filename(self, test_context):
        """Test output type inferred from name for filename/name."""
        functor = GlobTagFunctor()

        outputs = [
            FunctorOutput(name="name", options=[]),
            FunctorOutput(name="filename", options=[]),
            FunctorOutput(name="file", options=[]),
        ]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        for output_name in ["name", "filename", "file"]:
            assert output_name in result.outputs
            assert result.outputs[output_name].value == "test_v010.ma"

    def test_output_default_behavior(self, test_context):
        """Test default behavior for unknown output names."""
        functor = GlobTagFunctor()

        outputs = [FunctorOutput(name="unknown_output", options=[])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert "unknown_output" in result.outputs
        # Should default to name/filename
        assert result.outputs["unknown_output"].value == "test_v010.ma"

    def test_mixed_outputs(self, test_context):
        """Test mixed outputs with different types."""
        functor = GlobTagFunctor()

        outputs = [
            FunctorOutput(name="version", options=[]),
            FunctorOutput(name="file_name", options=["name"]),
            FunctorOutput(name="all_tags", options=["tags"]),
            FunctorOutput(name="first_tag", options=["tag"]),
        ]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert result.outputs["version"].value == 10
        assert result.outputs["file_name"].value == "test_v010.ma"
        assert isinstance(result.outputs["all_tags"].value, list)
        assert isinstance(result.outputs["first_tag"].value, str)

    def test_case_insensitive_options(self, test_context):
        """Test that options are case insensitive."""
        functor = GlobTagFunctor()

        outputs = [
            FunctorOutput(name="result1", options=["VERSION"]),
            FunctorOutput(name="result2", options=["Name"]),
            FunctorOutput(name="result3", options=["TAGS"]),
        ]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert result.outputs["result1"].value == 10
        assert result.outputs["result2"].value == "test_v010.ma"
        assert isinstance(result.outputs["result3"].value, list)


class TestGlobTagFunctorBackwardsCompatibility:
    """Test backwards compatibility with filename output."""

    @pytest.fixture
    def test_context(self):
        """Create a test context with temporary files."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Create test files
            (temp_path / "test_v001.ma").write_text("version 1")

            context = FunctorContext(
                parent=str(temp_path),
                variables=Box({"tags": {}}),
            )

            yield context

    def test_filename_output_still_works(self, test_context):
        """Test that 'filename' output name still works for backwards compatibility."""
        functor = GlobTagFunctor()

        outputs = [FunctorOutput(name="filename", options=[])]

        result = functor.parse(["test_v%TAG%.ma"], outputs, test_context)

        assert result.success
        assert "filename" in result.outputs
        assert result.outputs["filename"].value == "test_v001.ma"
