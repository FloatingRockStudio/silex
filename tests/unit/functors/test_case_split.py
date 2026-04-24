"""
Tests for the Case Split functors (CamelCase, SnakeCase, TitleCase).
"""

import pytest

pytest.importorskip("silex.functors", reason="silex C++ bindings not available")

from silex import Box

from silex.functors import SplitCamelCaseFunctor, SplitSnakeCaseFunctor
from silex import FunctorOutput, FunctorContext, ResolvedValue
from silex import FunctorParams


@pytest.fixture
def camelcase_functor():
    """Create a Split CamelCase functor instance."""
    return SplitCamelCaseFunctor()


@pytest.fixture
def snakecase_functor():
    """Create a Split SnakeCase functor instance."""
    return SplitSnakeCaseFunctor()


@pytest.fixture
def context():
    """Create a basic context for testing."""
    return FunctorContext(context=Box(), parent="/some/path", segment="test", variables=Box())


def _outputs_from_strings(*args):
    """Helper to create FunctorOutput instances from strings, splitting out options
    eg:
        character:?:1-5:2 -> character, options=['?', '1-5', '2']
    """
    outputs = []
    for arg in args:
        name, *options = arg.split(":")
        outputs.append(FunctorOutput(name=name, options=options))
    return outputs


def _outputs_from_strings_simple(output_names):
    """Helper to create FunctorOutput list from strings (simple version)."""
    return [FunctorOutput(name=name, options=[]) for name in output_names]


# MARK: CamelCase Tests
class TestSplitCamelCaseFunctorWrite:
    """Test the write functionality of Split CamelCase functor."""

    def test_write_basic(self, camelcase_functor, context):
        """Test basic write functionality."""
        result = camelcase_functor.format(["bob", "base"], context)
        assert result.success is True
        assert result.output == "bobBase"

    def test_write_single_part(self, camelcase_functor, context):
        """Test write with single part."""
        result = camelcase_functor.format(["bob"], context)
        assert result.success is True
        assert result.output == "bob"

    def test_write_multiple_parts(self, camelcase_functor, context):
        """Test write with multiple parts."""
        result = camelcase_functor.format(["chr", "bob", "bilby", "base"], context)
        assert result.success is True
        assert result.output == "chrBobBilbyBase"

    def test_write_empty_inputs(self, camelcase_functor, context):
        """Test write with empty inputs."""
        result = camelcase_functor.format([], context)
        assert result.success is False
        assert "No input values provided" in result.message

    def test_write_none_inputs(self, camelcase_functor, context):
        """Test write with None inputs."""
        result = camelcase_functor.format([None, "", "  "], context)
        assert result.success is False
        assert "Nothing to write" in result.message

    def test_write_mixed_inputs(self, camelcase_functor, context):
        """Test write with mixed valid/invalid inputs."""
        result = camelcase_functor.format(["bob", None, "", "base", "  "], context)
        assert result.success is True
        assert result.output == "bobBase"


class TestSplitCamelCaseFunctorRead:
    """Test the read functionality of Split CamelCase functor."""

    def test_read_basic_explicit_indices(self, camelcase_functor, context):
        """Test basic read with explicit indices."""
        outputs = _outputs_from_strings("character:0", "asset:1", "variant:2")
        result = camelcase_functor.parse(["chrBobBilby"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bob"
        assert result.outputs["variant"].value == "bilby"
        assert all(not v.is_ambiguous for v in result.outputs.values())

    def test_read_sequential_outputs(self, camelcase_functor, context):
        """Test read with sequential outputs (no explicit indices)."""
        outputs = _outputs_from_strings("character", "asset", "variant")
        result = camelcase_functor.parse(["chrBobBilby"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bob"
        assert result.outputs["variant"].value == "bilby"

    def test_read_with_optional(self, camelcase_functor, context):
        """Test read with optional outputs."""
        outputs = _outputs_from_strings("character:0", "asset:+", "variant:?")
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"
        assert result.outputs["variant"].value == "base"
        # Check ambiguity
        assert not result.outputs["character"].is_ambiguous  # Explicit index
        assert result.outputs["asset"].is_ambiguous  # Greedy with optional after it
        assert not result.outputs["variant"].is_ambiguous  # Gets what's left

    def test_read_greedy_behavior(self, camelcase_functor, context):
        """Test greedy output behavior."""
        outputs = [FunctorOutput("character"), FunctorOutput("asset", ["+"]), FunctorOutput("variant")]  # greedy
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"  # Takes maximum possible
        assert result.outputs["variant"].value == "base"
        # Check ambiguity
        assert not result.outputs["character"].is_ambiguous  # Single part, sequential
        assert result.outputs["asset"].is_ambiguous  # Greedy with optional after it
        assert not result.outputs["variant"].is_ambiguous  # Gets what's left

    def test_read_with_context_values(self, camelcase_functor):
        """Test read with known context values."""
        context_with_values = FunctorContext(
            context=Box(asset="bobBilby"), parent="/some/path", segment="test", variables=Box()
        )
        outputs = _outputs_from_strings("character", "asset:+", "variant:?")
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context_with_values)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"  # Matches context
        assert result.outputs["variant"].value == "base"

    def test_read_negative_indices(self, camelcase_functor, context):
        """Test read with negative indices."""
        outputs = _outputs_from_strings("character:0", "variant:-1")
        result = camelcase_functor.parse(["chrBobBilby"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["variant"].value == "bilby"

    def test_read_range_indices(self, camelcase_functor, context):
        """Test read with range indices."""
        outputs = _outputs_from_strings("character:0", "asset:1-2")
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"

    def test_read_empty_input(self, camelcase_functor, context):
        """Test read with empty input."""
        outputs = _outputs_from_strings("test")
        result = camelcase_functor.parse([""], outputs, context)
        assert result.success is True
        assert len(result.outputs) == 0

    def test_read_no_inputs(self, camelcase_functor, context):
        """Test read with no inputs."""
        outputs = _outputs_from_strings("test")
        result = camelcase_functor.parse([], outputs, context)
        assert result.success is False
        assert "No input value provided" in result.message

    def test_read_multiple_inputs_error(self, camelcase_functor, context):
        """Test read with multiple inputs (should fail)."""
        outputs = _outputs_from_strings("test")
        result = camelcase_functor.parse(["input1", "input2"], outputs, context)
        assert result.success is False
        assert "expects exactly one input" in result.message

    def test_read_no_outputs(self, camelcase_functor, context):
        """Test read with no outputs."""
        result = camelcase_functor.parse(["chrBobBilby"], [], context)
        assert result.success is False
        assert "No outputs specified" in result.message

    def test_read_invalid_index(self, camelcase_functor, context):
        """Test read with invalid index."""
        outputs = _outputs_from_strings("test:10")  # Index out of bounds
        result = camelcase_functor.parse(["chrBobBilby"], outputs, context)
        assert result.success is False
        assert "Invalid index range" in result.message

    def test_read_optional_invalid_index(self, camelcase_functor, context):
        """Test read with optional output having invalid index."""
        outputs = _outputs_from_strings("character:0", "missing:10:?")  # Invalid index but optional
        result = camelcase_functor.parse(["chrBobBilby"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert "missing" not in result.outputs

    def test_read_context_mismatch(self, camelcase_functor):
        """Test read with context value that doesn't match parts."""
        context_with_values = FunctorContext(
            context=Box(character="wrong"), parent="/some/path", segment="test", variables=Box()
        )
        outputs = _outputs_from_strings("character:0")
        result = camelcase_functor.parse(["chrBobBilby"], outputs, context_with_values)
        assert result.success is False
        assert "doesn't match parts" in result.message

    def test_read_nested_context_key(self, camelcase_functor):
        """Test read with nested context key like 'project.code'."""
        context_with_nested = FunctorContext(
            context=Box(project=Box(code="bob")), parent="/some/path", segment="test", variables=Box()
        )
        outputs = _outputs_from_strings("character:0", "project.code:+", "variant:?:+")
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context_with_nested)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["project.code"].value == "bob"
        assert result.outputs["variant"].value == "bilbyBase"  # Gets remaining parts as greedy optional

    def test_read_complex_scenario(self, camelcase_functor, context):
        """Test the complex scenario from the requirements."""
        # chrBobBilbyBase with character:0, asset:+, variant:?
        outputs = _outputs_from_strings("character:0", "asset:+", "variant:?")
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"  # Takes maximum while leaving minimum for variant
        assert result.outputs["variant"].value == "base"

    def test_read_all_optional_greedy_balanced(self, camelcase_functor, context):
        """Test behavior when multiple outputs are optional and greedy."""
        outputs = _outputs_from_strings("character:0", "asset:+:?", "variant:+:?")
        result = camelcase_functor.parse(["chrBobBilbySockPuppet"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"
        assert result.outputs["variant"].value == "sockPuppet"
        # Check ambiguity
        assert not result.outputs["character"].is_ambiguous  # Explicit index
        assert result.outputs["asset"].is_ambiguous  # Greedy with optional after it
        assert result.outputs["variant"].is_ambiguous  # Optional greedy

    def test_read_all_optional_greedy_unbalanced(self, camelcase_functor, context):
        """Test behavior when multiple outputs are optional and greedy."""
        outputs = _outputs_from_strings("character:0", "asset:+:?", "variant:+:?")
        result = camelcase_functor.parse(["chrBobBilbyBase"], outputs, context)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        # With only 3 parts left, they're divided roughly equally (1,1,1 or 2,1)
        # The current logic gives each optional greedy output 1 part initially
        # But variant gets the remaining parts after asset takes its share
        assert result.outputs["asset"].value == "bob"
        assert result.outputs["variant"].value == "bilbyBase"

    def test_read_no_parts_found(self, camelcase_functor, context):
        """Test read when no camelCase parts are found."""
        outputs = _outputs_from_strings("test")
        result = camelcase_functor.parse(["123"], outputs, context)  # No valid camelCase parts
        assert result.success is False
        assert "No parts found" in result.message

    def test_read_context_resolves_ambiguity(self, camelcase_functor):
        """Test that context can resolve ambiguous situations."""
        # Without context: chrBobBilbyDirtyFace -> chr, bobBilbyDirty, face (ambiguous)
        # With context: asset=bobBilby -> chr, bobBilby, dirtyFace (unambiguous)

        # First test without context (ambiguous)
        context_without = FunctorContext(context=Box(), parent="/some/path", segment="test", variables=Box())
        outputs = _outputs_from_strings("character:0", "asset:+", "variant:?:+")
        result = camelcase_functor.parse(["chrBobBilbyDirtyFace"], outputs, context_without)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilbyDirty"
        assert result.outputs["variant"].value == "face"
        assert not result.outputs["character"].is_ambiguous  # Explicit index
        assert result.outputs["asset"].is_ambiguous  # Could be different length
        assert not result.outputs["variant"].is_ambiguous  # Only one part left

        # Now test with context (unambiguous)
        context_with_asset = FunctorContext(
            context=Box(asset="BobBilby"),  # Different case, should still match
            parent="/some/path",
            segment="test",
            variables=Box(),
        )
        result = camelcase_functor.parse(["chrBobBilbyDirtyFace"], outputs, context_with_asset)
        assert result.success is True
        assert result.outputs["character"].value == "chr"
        assert result.outputs["asset"].value == "bobBilby"  # Matches context
        assert result.outputs["variant"].value == "dirtyFace"  # Gets remaining parts
        assert not result.outputs["character"].is_ambiguous  # Explicit index
        assert not result.outputs["asset"].is_ambiguous  # Known from context
        assert result.outputs["variant"].is_ambiguous  # Optional greedy that took multiple parts


# MARK: SnakeCase Tests
class TestSplitSnakeCaseFunctorWrite:
    """Test the write functionality of Split SnakeCase functor."""

    def test_write_basic(self, snakecase_functor, context):
        """Test basic write functionality."""
        result = snakecase_functor.format(["bob", "base"], context)
        assert result.success
        assert result.output == "bob_base"

    def test_write_single_part(self, snakecase_functor, context):
        """Test write with single part."""
        result = snakecase_functor.format(["test"], context)
        assert result.success
        assert result.output == "test"

    def test_write_multiple_parts(self, snakecase_functor, context):
        """Test write with multiple parts."""
        result = snakecase_functor.format(["hello", "world", "test"], context)
        assert result.success
        assert result.output == "hello_world_test"

    def test_write_empty_inputs(self, snakecase_functor, context):
        """Test write with empty inputs."""
        result = snakecase_functor.format([], context)
        assert not result.success
        assert "No input values" in result.message


class TestSplitSnakeCaseFunctorRead:
    """Test the read functionality of Split SnakeCase functor."""

    def test_read_basic_split(self, snakecase_functor, context):
        """Test basic splitting of snake_case string."""
        outputs = _outputs_from_strings_simple(["a", "b"])
        result = snakecase_functor.parse(["bob_base"], outputs, context)
        assert result.success
        assert result.outputs["a"].value == "bob"
        assert result.outputs["b"].value == "base"

    def test_read_single_output(self, snakecase_functor, context):
        """Test reading into single output."""
        outputs = _outputs_from_strings_simple(["combined"])
        result = snakecase_functor.parse(["bob_base"], outputs, context)
        assert result.success
        assert result.outputs["combined"].value == "bob"

    def test_read_explicit_index(self, snakecase_functor, context):
        """Test reading with explicit index."""
        outputs = [FunctorOutput(name="second", options=["1"])]
        result = snakecase_functor.parse(["bob_base"], outputs, context)
        assert result.success
        assert result.outputs["second"].value == "base"

    def test_read_greedy_output(self, snakecase_functor, context):
        """Test reading with greedy output."""
        outputs = [FunctorOutput(name="all", options=[FunctorParams.GREEDY])]
        result = snakecase_functor.parse(["bob_base_test"], outputs, context)
        assert result.success
        assert result.outputs["all"].value == "bob_base_test"

    def test_read_optional_output(self, snakecase_functor, context):
        """Test reading with optional output."""
        outputs = [
            FunctorOutput(name="first", options=[]),
            FunctorOutput(name="second", options=[FunctorParams.OPTIONAL]),
            FunctorOutput(name="third", options=[FunctorParams.OPTIONAL]),
        ]
        result = snakecase_functor.parse(["bob"], outputs, context)
        assert result.success
        assert result.outputs["first"].value == "bob"
        assert "second" not in result.outputs
        assert "third" not in result.outputs
