"""
Tests for the Lexicon functor.
"""

import pytest

pytest.importorskip("silex.functors", reason="silex C++ bindings not available")

from silex import Box

from silex.functors import LexiconFunctor
from silex import FunctorOutput, FunctorContext


@pytest.fixture
def functor():
    """Create a Lexicon functor instance."""
    return LexiconFunctor()


@pytest.fixture
def context_with_lexicon():
    """Create a context with lexicon data."""
    lexicon_data = {
        "classification": {"character": ["chr", "char"], "background": ["bg", "bgd"], "animation": "anim"},
        "asset_type": {"model": ["mdl", "mod"], "texture": "tex"},
    }
    return FunctorContext(
        context=Box({}), parent="/some/path", segment="test", variables=Box(lexicon=lexicon_data)
    )


@pytest.fixture
def context_empty():
    """Create a context without lexicon data."""
    return FunctorContext(context=Box({}), parent="/some/path", segment="test", variables=Box())


def _outputs_from_strings(output_names):
    """Helper to create FunctorOutput list from strings."""
    if isinstance(output_names, str):
        output_names = [output_names]
    return [FunctorOutput(name=name, options=[]) for name in output_names]


class TestLexiconWrite:
    """Test the write functionality of Lexicon functor (updated behavior).

    The current implementation expects inputs in the form [category, value].
    It returns success only when the category exists and the full name matches
    exactly (case-sensitive on the provided value). Missing categories or values
    produce failure results.
    """

    def test_write_full_name_to_abbreviation(self, functor, context_with_lexicon):
        result = functor.format(["classification", "character"], context_with_lexicon)
        assert result.success
        assert result.output == "chr"

    def test_write_full_name_to_single_abbreviation(self, functor, context_with_lexicon):
        result = functor.format(["classification", "animation"], context_with_lexicon)
        assert result.success
        assert result.output == "anim"

    def test_write_case_sensitive_behavior(self, functor, context_with_lexicon):
        # Implementation compares full_name.lower() == value (value not lowered),
        # so passing an uppercased value should fail.
        result = functor.format(["classification", "CHARACTER"], context_with_lexicon)
        assert not result.success
        assert "No Lexicon format value" in result.message

    def test_write_not_found_returns_failure(self, functor, context_with_lexicon):
        result = functor.format(["classification", "unknown"], context_with_lexicon)
        assert not result.success
        assert "No Lexicon format value" in result.message

    def test_write_no_lexicon_returns_failure(self, functor, context_empty):
        result = functor.format(["classification", "character"], context_empty)
        assert not result.success
        assert "No lexicon category for classification" in result.message

    def test_write_empty_input(self, functor, context_with_lexicon):
        result = functor.format([], context_with_lexicon)
        assert not result.success
        assert "No input value" in result.message


class TestLexiconRead:
    """Test the read functionality of Lexicon functor (updated behavior).

    The current implementation returns failure if any requested output category
    does not contain a mapping for the provided abbreviation. Tests are adjusted
    to reflect this stricter behavior.
    """

    def test_read_abbreviation_to_full_name(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("classification")
        result = functor.parse(["chr"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["classification"].value == "character"

    def test_read_single_abbreviation(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("classification")
        result = functor.parse(["anim"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["classification"].value == "animation"

    def test_read_single_output_success_only(self, functor, context_with_lexicon):
        # Requesting only the classification output should succeed for 'chr'
        outputs = _outputs_from_strings(["classification"])
        result = functor.parse(["chr"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["classification"].value == "character"

    def test_read_case_insensitive(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("classification")
        result = functor.parse(["CHR"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["classification"].value == "character"

    def test_read_not_found_returns_failure(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("classification")
        result = functor.parse(["unknown"], outputs, context_with_lexicon)
        assert not result.success
        assert "No Lexicon parse value" in result.message

    def test_read_no_lexicon_returns_failure(self, functor, context_empty):
        outputs = _outputs_from_strings("classification")
        result = functor.parse(["chr"], outputs, context_empty)
        assert not result.success
        assert "No Lexicon parse value" in result.message

    def test_read_empty_input(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("classification")
        result = functor.parse([], outputs, context_with_lexicon)
        assert not result.success
        assert "No input value" in result.message

    def test_read_no_outputs(self, functor, context_with_lexicon):
        result = functor.parse(["chr"], [], context_with_lexicon)
        assert not result.success
        assert "No output categories" in result.message

    def test_read_different_categories(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("asset_type")
        result = functor.parse(["mdl"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["asset_type"].value == "model"

    def test_read_alternative_abbreviation(self, functor, context_with_lexicon):
        outputs = _outputs_from_strings("classification")
        result = functor.parse(["char"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["classification"].value == "character"

    def test_read_optional_output_falls_back_to_input(self, functor, context_with_lexicon):
        outputs = [FunctorOutput(name="classification", options=["?"])]
        result = functor.parse(["mr"], outputs, context_with_lexicon)
        assert result.success
        assert result.outputs["classification"].value == "mr"
