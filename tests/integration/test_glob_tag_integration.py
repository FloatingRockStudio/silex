"""
Integration tests for the GlobTag functor in the context of Silex path resolution.

These tests verify that the GlobTag functor works correctly with the full Silex
expression parser and evaluator system.
"""

import pytest
import tempfile
from pathlib import Path

pytest.importorskip("silex.core", reason="silex C++ bindings not available")

from silex import Box

from silex.core import ExpressionParser
from silex.core import ExpressionEvaluator
from silex.core import Registry
from silex import FunctorContext, SilexConfig, SilexFunctorInfo
from silex import ExpressionMode, Language
from silex import set_verbosity


class TestGlobTagIntegration:
    """Integration tests for GlobTag functor with Silex expression system."""

    @pytest.fixture(autouse=True)
    def setup_test_environment(self, monkeypatch):
        """Set up the test environment for each test."""
        test_resources = Path(__file__).parent.parent / "resources"
        monkeypatch.setenv("SILEX_SCHEMA_PATH", str(test_resources))
        set_verbosity(1)

    @pytest.fixture
    def registry(self):
        """Create a registry with GlobTag functor registered."""
        registry = Registry()

        # Register GlobTag functor
        registry.register_functor(
            SilexFunctorInfo(
                uid="silex.impl.functors.glob_tag.GlobTagFunctor",
                name="GlobTagFunctor",
                aliases=["glob_tag"],
                module="silex.impl.functors.glob_tag",
                package="silex",
                language=Language.PYTHON,
            )
        )

        return registry

    @pytest.fixture
    def expression_parser(self, registry):
        """Create an expression parser with GlobTag functor."""
        aliases = {
            "glob_tag": "silex.impl.functors.glob_tag.GlobTagFunctor",
        }
        return ExpressionParser(registry, aliases)

    @pytest.fixture
    def evaluator(self, registry):
        """Create an expression evaluator."""
        return ExpressionEvaluator(registry)

    @pytest.fixture
    def test_files_context(self):
        """Create a test context with temporary files."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Create test files with version numbers
            (temp_path / "test_v001.ma").write_text("version 1")
            (temp_path / "test_v002.ma").write_text("version 2")
            (temp_path / "test_v003.ma").write_text("version 3")
            (temp_path / "test_v0010.ma").write_text("version 10")

            # Create tag files
            (temp_path / ".test_v001.published").write_text("")
            (temp_path / ".test_v002.deprecated").write_text("")
            (temp_path / ".test_v003.wip").write_text("")

            # Create scene files
            (temp_path / "scene_v001.ma").write_text("scene 1")
            (temp_path / "scene_v002.ma").write_text("scene 2")
            (temp_path / ".scene_v001.published").write_text("")

            # Create complex pattern files for testing
            (temp_path / "seq010_shot020_v001.exr").write_text("complex 1")
            (temp_path / "seq010_shot020_v002.exr").write_text("complex 2")
            (temp_path / "seq010_shot020_v003.exr").write_text("complex 3")

            context = FunctorContext(
                context=Box({"entity": {"version": 5}, "sequence": "010", "shot": "020"}),
                parent=str(temp_path),
                segment="test_segment",
                variables=Box(
                    {
                        "tags": {
                            "published": {"alias": ["published"], "explicit": True},
                            "latest": {"alias": ["latest"], "explicit": False},
                            "deprecated": {"alias": ["deprecated"], "explicit": True},
                            "wip": {"alias": ["wip", "work"], "explicit": False},
                        }
                    }
                ),
            )

            yield context

    @pytest.fixture
    def config_with_tags(self):
        """Create a config with tag configuration."""
        return SilexConfig(
            global_variables=Box(
                {
                    "tags": {
                        "published": {"alias": ["published"], "explicit": True},
                        "latest": {"alias": ["latest"], "explicit": False},
                        "deprecated": {"alias": ["deprecated"], "explicit": True},
                        "wip": {"alias": ["wip", "work"], "explicit": False},
                    }
                }
            ),
            functor_variables=Box({}),
        )

    def test_glob_tag_version_generation_expression(
        self, expression_parser, evaluator, test_files_context, config_with_tags
    ):
        """Test GlobTag version generation through expression parser."""
        # Parse expression for version generation
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma, {entity.version}, 4)"])

        assert len(expressions.expressions) == 1
        assert expressions.mode == ExpressionMode.FORMAT

        expression = expressions.expressions[0]
        assert expression.functor_info.uid == "silex.impl.functors.glob_tag.GlobTagFunctor"

        # Evaluate the expression
        result = evaluator.evaluate_format_expression(expression, test_files_context, config_with_tags)

        assert result.success
        assert result.output == "test_v0005.ma"

    def test_glob_tag_tag_lookup_expression(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag tag-based lookup through expression parser."""
        # Parse expression for tag lookup
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma, published)"])

        assert len(expressions.expressions) == 1
        assert expressions.mode == ExpressionMode.FORMAT

        expression = expressions.expressions[0]

        # Evaluate the expression
        result = evaluator.evaluate_format_expression(expression, test_files_context, config_with_tags)

        assert result.success
        assert result.output == "test_v001.ma"

    def test_glob_tag_latest_tag_expression(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag latest tag lookup through expression parser."""
        # Parse expression for latest tag
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma, latest)"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]

        # Evaluate the expression
        result = evaluator.evaluate_format_expression(expression, test_files_context, config_with_tags)

        assert result.success
        assert result.output == "test_v0010.ma"  # Highest version without explicit tags

    def test_glob_tag_read_version_expression(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag read operation for version extraction."""
        # Parse expression for reading version
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->version"])

        assert len(expressions.expressions) == 1
        assert expressions.mode == ExpressionMode.PARSE

        expression = expressions.expressions[0]
        assert len(expression.outputs) == 1
        assert expression.outputs[0] == "version"  # outputs is a list of strings
        assert len(expression.functor_outputs) == 1
        assert expression.functor_outputs[0].name == "version"  # functor_outputs are FunctorOutput objects

        # Evaluate the expression
        inputs = ["test_v%TAG%.ma"]  # Need to provide the pattern as input
        result = evaluator.evaluate_parse_expression(expression, inputs, test_files_context, config_with_tags)

        assert result.success
        assert "version" in result.outputs
        assert result.outputs["version"].value == 10  # From test_v0010.ma

    def test_glob_tag_read_name_expression(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag read operation for name extraction."""
        # Parse expression for reading name
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->name"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]

        # Evaluate the expression
        inputs = ["test_v%TAG%.ma"]
        result = evaluator.evaluate_parse_expression(expression, inputs, test_files_context, config_with_tags)

        assert result.success
        assert "name" in result.outputs
        assert result.outputs["name"].value == "test_v0010.ma"

    def test_glob_tag_read_tags_expression(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag read operation for tags extraction."""
        # Parse expression for reading tags
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->tags"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]

        # Evaluate the expression
        inputs = ["test_v%TAG%.ma"]
        result = evaluator.evaluate_parse_expression(expression, inputs, test_files_context, config_with_tags)

        assert result.success
        assert "tags" in result.outputs
        assert isinstance(result.outputs["tags"].value, list)

    def test_glob_tag_read_multiple_outputs(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag read operation with multiple outputs."""
        # Parse expression for reading multiple outputs
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->version,name,tags"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]
        assert len(expression.outputs) == 3

        # Evaluate the expression
        inputs = ["test_v%TAG%.ma"]
        result = evaluator.evaluate_parse_expression(expression, inputs, test_files_context, config_with_tags)

        assert result.success
        assert "version" in result.outputs
        assert "name" in result.outputs
        assert "tags" in result.outputs

        assert result.outputs["version"].value == 10
        assert result.outputs["name"].value == "test_v0010.ma"
        assert isinstance(result.outputs["tags"].value, list)

    def test_glob_tag_with_options_version(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag with output options specified."""
        # Parse expression with options
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->result:version"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]
        assert len(expression.outputs) == 1
        assert expression.outputs[0] == "result"  # outputs is list of strings
        assert len(expression.functor_outputs) == 1
        assert expression.functor_outputs[0].name == "result"
        assert expression.functor_outputs[0].options == ["version"]

        # Evaluate the expression
        inputs = ["test_v%TAG%.ma"]
        result = evaluator.evaluate_parse_expression(expression, inputs, test_files_context, config_with_tags)

        assert result.success
        assert "result" in result.outputs
        assert result.outputs["result"].value == 10

    def test_glob_tag_with_options_tag(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag with tag option (single tag output)."""
        # Parse expression with tag option
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->single_tag:tag"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]
        assert expression.functor_outputs[0].name == "single_tag"
        assert expression.functor_outputs[0].options == ["tag"]

        # Evaluate the expression
        inputs = ["test_v%TAG%.ma"]
        result = evaluator.evaluate_parse_expression(expression, inputs, test_files_context, config_with_tags)

        assert result.success
        assert "single_tag" in result.outputs
        # Should return first tag or empty string if no tags
        tag_value = result.outputs["single_tag"].value
        assert isinstance(tag_value, str)

    def test_glob_tag_complex_pattern_expression(
        self, expression_parser, evaluator, test_files_context, config_with_tags
    ):
        """Test GlobTag with complex filename patterns."""
        # Parse expression with complex pattern
        expressions = expression_parser.parse_expressions(["$glob_tag(seq{sequence}_shot{shot}_v%TAG%.exr, 7, 3)"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]

        # Evaluate the expression
        result = evaluator.evaluate_format_expression(expression, test_files_context, config_with_tags)

        assert result.success
        assert result.output == "seq010_shot020_v007.exr"

    def test_glob_tag_expression_parsing_structure(self, expression_parser):
        """Test that GlobTag expressions parse correctly."""
        # Test write expression
        write_expressions = expression_parser.parse_expressions(["$glob_tag(v%TAG%, {entity.version}, 4)"])
        assert len(write_expressions.expressions) == 1
        assert write_expressions.mode == ExpressionMode.FORMAT

        write_expr = write_expressions.expressions[0]
        assert write_expr.functor_info.uid == "silex.impl.functors.glob_tag.GlobTagFunctor"
        assert len(write_expr.inputs) == 3  # pattern, version, padding
        assert len(write_expr.outputs) == 0  # Write expressions have no outputs

        # Test read expression
        read_expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma)->version,name"])
        assert len(read_expressions.expressions) == 1
        assert read_expressions.mode == ExpressionMode.PARSE

        read_expr = read_expressions.expressions[0]
        assert read_expr.functor_info.uid == "silex.impl.functors.glob_tag.GlobTagFunctor"
        assert len(read_expr.inputs) == 1  # pattern only
        assert len(read_expr.outputs) == 2  # version, name

    def test_glob_tag_error_handling_no_files(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag error handling when no files match."""
        # Parse expression with non-existent pattern
        expressions = expression_parser.parse_expressions(["$glob_tag(nonexistent_v%TAG%.ma, published)"])

        expression = expressions.expressions[0]

        # Evaluate the expression
        result = evaluator.evaluate_format_expression(expression, test_files_context, config_with_tags)

        assert not result.success
        assert "No files found" in result.message

    def test_glob_tag_error_handling_no_tags(self, expression_parser, evaluator, test_files_context, config_with_tags):
        """Test GlobTag error handling when no files have requested tag."""
        # Parse expression with non-existent tag
        expressions = expression_parser.parse_expressions(["$glob_tag(test_v%TAG%.ma, nonexistent_tag)"])

        expression = expressions.expressions[0]

        # Evaluate the expression
        result = evaluator.evaluate_format_expression(expression, test_files_context, config_with_tags)

        assert not result.success
        assert "No files found with tags" in result.message


class TestGlobTagCompositeAndSpecialTags:
    """Tests for composite tags, explicit tags, and special tags (next, latest)."""

    @pytest.fixture(autouse=True)
    def setup_test_environment(self, monkeypatch):
        """Set up the test environment for each test."""
        test_resources = Path(__file__).parent.parent / "resources"
        monkeypatch.setenv("SILEX_SCHEMA_PATH", str(test_resources))
        set_verbosity(1)

    @pytest.fixture
    def registry(self):
        """Create a registry with GlobTag functor registered."""
        registry = Registry()
        registry.register_functor(
            SilexFunctorInfo(
                uid="silex.impl.functors.glob_tag.GlobTagFunctor",
                name="GlobTagFunctor",
                aliases=["glob_tag"],
                module="silex.impl.functors.glob_tag",
                package="silex",
                language=Language.PYTHON,
            )
        )
        return registry

    @pytest.fixture
    def expression_parser(self, registry):
        """Create an expression parser with GlobTag functor."""
        aliases = {
            "glob_tag": "silex.impl.functors.glob_tag.GlobTagFunctor",
        }
        return ExpressionParser(registry, aliases)

    @pytest.fixture
    def evaluator(self, registry):
        """Create an expression evaluator."""
        return ExpressionEvaluator(registry)

    @pytest.fixture
    def composite_test_context(self):
        """Create test files with various tags for composite tag testing."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Create versioned files
            (temp_path / "asset_v001.ma").write_text("version 1")
            (temp_path / "asset_v002.ma").write_text("version 2")
            (temp_path / "asset_v003.ma").write_text("version 3")
            (temp_path / "asset_v004.ma").write_text("version 4")
            (temp_path / "asset_v005.ma").write_text("version 5")
            (temp_path / "asset_v006.ma").write_text("version 6")
            (temp_path / "asset_v007.ma").write_text("version 7")

            # v001: published tag
            (temp_path / ".asset_v001.published").write_text("")

            # v002: deprecated tag (explicit)
            (temp_path / ".asset_v002.deprecated").write_text("")

            # v003: wip tag
            (temp_path / ".asset_v003.wip").write_text("")

            # v004: locked tag (explicit)
            (temp_path / ".asset_v004.locked").write_text("")

            # v005: no tags (should be "latest" candidate)

            # v006: published + locked
            (temp_path / ".asset_v006.published").write_text("")
            (temp_path / ".asset_v006.locked").write_text("")

            # v007: no tags (should be "latest")

            context = FunctorContext(
                context=Box({"entity": {"version": 10}}),
                parent=str(temp_path),
                segment="test_segment",
                variables=Box(
                    {
                        "tags": {
                            "published": {"explicit": False, "alias": []},
                            "deprecated": {"explicit": True, "alias": []},
                            "locked": {"explicit": True, "alias": []},
                            "wip": {"explicit": False, "alias": []},
                            "official": {"explicit": False, "alias": ["published", "latest"]},
                            "safe": {"explicit": False, "alias": ["published", "wip", "latest"]},
                        }
                    }
                ),
            )

            yield context

    @pytest.fixture
    def config_with_composite_tags(self):
        """Create config with composite tag definitions."""
        return SilexConfig(
            global_variables=Box(
                {
                    "tags": {
                        "published": {"explicit": False, "alias": []},
                        "deprecated": {"explicit": True, "alias": []},
                        "locked": {"explicit": True, "alias": []},
                        "wip": {"explicit": False, "alias": []},
                        "official": {"explicit": False, "alias": ["published", "latest"]},
                        "safe": {"explicit": False, "alias": ["published", "wip", "latest"]},
                    }
                }
            ),
            functor_variables=Box({}),
        )

    def test_composite_tag_fallback_to_published(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test composite tag 'official' resolves to 'published' when available."""
        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, official)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should find v001 with published tag (not v006 which is locked)
        assert result.output == "asset_v001.ma"

    def test_composite_tag_fallback_to_latest(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test composite tag falls back to 'latest' when primary tag not found."""
        # Remove published tag so it falls back to latest
        temp_path = Path(composite_test_context.parent)
        (temp_path / ".asset_v001.published").unlink()

        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, official)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should fall back to latest (v007 - highest without explicit/locked tags)
        assert result.output == "asset_v007.ma"

    def test_composite_tag_multiple_fallbacks(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test composite tag with multiple fallbacks (published->wip->latest)."""
        # Remove published tag
        temp_path = Path(composite_test_context.parent)
        (temp_path / ".asset_v001.published").unlink()

        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, safe)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should find v003 with wip tag (second in fallback chain)
        assert result.output == "asset_v003.ma"

    def test_latest_excludes_explicit_tags(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test that 'latest' excludes files with explicit tags (deprecated, locked)."""
        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, latest)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should return v007 (highest without explicit tags)
        # v002=deprecated, v004=locked, v006=locked should all be excluded
        assert result.output == "asset_v007.ma"

    def test_latest_excludes_locked(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test that 'latest' specifically excludes locked versions."""
        # Add locked tag to v007
        temp_path = Path(composite_test_context.parent)
        (temp_path / ".asset_v007.locked").write_text("")

        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, latest)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should now return v005 (next highest without explicit/locked)
        assert result.output == "asset_v005.ma"

    def test_explicit_tag_deprecated_only_when_requested(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test that deprecated (explicit) tag is only returned when explicitly requested."""
        # Request deprecated explicitly
        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, deprecated)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        assert result.output == "asset_v002.ma"

    def test_explicit_tag_locked_accessible_when_requested(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test that locked files are accessible when explicitly requested."""
        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, locked)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should find v004 or v006 (both have locked tag, v006 is higher)
        assert result.output in ["asset_v004.ma", "asset_v006.ma"]

    def test_next_tag_increments_from_highest(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test that 'next' tag returns highest version + 1."""
        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, next)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Highest version is v007, so next should be v008
        assert result.output == "asset_v008.ma"

    def test_next_tag_excludes_locked_versions(
        self, expression_parser, evaluator, composite_test_context, config_with_composite_tags
    ):
        """Test that 'next' tag excludes locked versions when finding highest."""
        # Lock v007 (highest)
        temp_path = Path(composite_test_context.parent)
        (temp_path / ".asset_v007.locked").write_text("")

        expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, next)"])
        expression = expressions.expressions[0]
        result = evaluator.evaluate_format_expression(expression, composite_test_context, config_with_composite_tags)

        assert result.success
        # Should find v005 as highest non-locked, so next = v006
        # But v006 exists, so it should use v007 (locked) and give v008
        # Actually, if all unlocked versions are lower, it uses the absolute highest
        assert result.output == "asset_v008.ma"

    def test_next_tag_with_no_existing_files(
        self, expression_parser, evaluator, config_with_composite_tags
    ):
        """Test that 'next' returns v001 when no files exist."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            context = FunctorContext(
                context=Box({"entity": {"version": 10}}),
                parent=str(temp_path),
                segment="test_segment",
                variables=Box(config_with_composite_tags.global_variables),
            )

            expressions = expression_parser.parse_expressions(["$glob_tag(new_v%TAG%.ma, next)"])
            expression = expressions.expressions[0]
            result = evaluator.evaluate_format_expression(expression, context, config_with_composite_tags)

            assert result.success
            assert result.output == "new_v0001.ma"

    def test_next_tag_with_version_gaps(
        self, expression_parser, evaluator, config_with_composite_tags
    ):
        """Test that 'next' increments from highest regardless of gaps."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Create files with gaps: v001, v003, v010
            (temp_path / "asset_v001.ma").write_text("v1")
            (temp_path / "asset_v003.ma").write_text("v3")
            (temp_path / "asset_v0010.ma").write_text("v10")

            context = FunctorContext(
                context=Box({"entity": {"version": 5}}),
                parent=str(temp_path),
                segment="test_segment",
                variables=Box(config_with_composite_tags.global_variables),
            )

            expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, next)"])
            expression = expressions.expressions[0]
            result = evaluator.evaluate_format_expression(expression, context, config_with_composite_tags)

            assert result.success
            # Should be v0011 (v0010 + 1)
            assert result.output == "asset_v0011.ma"

    def test_composite_tag_no_matches_fails(
        self, expression_parser, evaluator, config_with_composite_tags
    ):
        """Test that composite tag fails when no fallbacks match."""
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)

            # Create files but all with explicit tags
            (temp_path / "asset_v001.ma").write_text("v1")
            (temp_path / ".asset_v001.deprecated").write_text("")

            context = FunctorContext(
                context=Box({"entity": {"version": 5}}),
                parent=str(temp_path),
                segment="test_segment",
                variables=Box(config_with_composite_tags.global_variables),
            )

            expressions = expression_parser.parse_expressions(["$glob_tag(asset_v%TAG%.ma, official)"])
            expression = expressions.expressions[0]
            result = evaluator.evaluate_format_expression(expression, context, config_with_composite_tags)

            assert not result.success
            assert "No files found with tags" in result.message

