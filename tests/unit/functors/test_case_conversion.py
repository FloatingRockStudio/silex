"""
Tests for the case conversion functors.
"""

import pytest

pytest.importorskip("silex.functors", reason="silex C++ bindings not available")

from silex import Box

from silex.functors import ConvertLowerCaseFunctor, ConvertUpperCaseFunctor, ConvertTitleCaseFunctor
from silex import FunctorOutput, FunctorContext


@pytest.fixture
def context():
    """Create a basic context for testing."""
    return FunctorContext(context=Box({}), parent="/some/path", segment="test", variables=Box())


def _outputs_from_strings(output_names):
    """Helper to create FunctorOutput list from strings."""
    if isinstance(output_names, str):
        output_names = [output_names]
    return [FunctorOutput(name=name, options=[]) for name in output_names]


class TestConvertLowerCaseFunctor:
    """Test the LowerCase functor."""

    @pytest.fixture
    def functor(self):
        return ConvertLowerCaseFunctor()

    def test_write_basic(self, functor, context):
        """Test basic write functionality."""
        result = functor.format(["Hello"], context)
        assert result.success
        assert result.output == "hello"

    def test_write_mixed_case(self, functor, context):
        """Test write with mixed case input."""
        result = functor.format(["HeLLo WoRLd"], context)
        assert result.success
        assert result.output == "hello world"

    def test_write_already_lowercase(self, functor, context):
        """Test write with already lowercase input."""
        result = functor.format(["hello"], context)
        assert result.success
        assert result.output == "hello"

    def test_write_numbers_and_symbols(self, functor, context):
        """Test write with numbers and symbols."""
        result = functor.format(["Hello123!@#"], context)
        assert result.success
        assert result.output == "hello123!@#"

    def test_write_empty_input(self, functor, context):
        """Test write with empty input."""
        result = functor.format([], context)
        assert not result.success
        assert "No input value" in result.message

    def test_read_basic(self, functor, context):
        """Test basic read functionality."""
        outputs = _outputs_from_strings("result")
        result = functor.parse(["Hello"], outputs, context)
        assert result.success
        assert result.outputs["result"].value == "hello"

    def test_read_multiple_outputs(self, functor, context):
        """Test read with multiple outputs."""
        outputs = _outputs_from_strings(["result1", "result2"])
        result = functor.parse(["Hello"], outputs, context)
        assert result.success
        assert result.outputs["result1"].value == "hello"
        assert result.outputs["result2"].value == "hello"

    def test_read_empty_input(self, functor, context):
        """Test read with empty input."""
        outputs = _outputs_from_strings("result")
        result = functor.parse([], outputs, context)
        assert not result.success
        assert "No input value" in result.message


class TestUpperConvertCaseFunctor:
    """Test the UpperCase functor."""

    @pytest.fixture
    def functor(self):
        return ConvertUpperCaseFunctor()

    def test_write_basic(self, functor, context):
        """Test basic write functionality."""
        result = functor.format(["hello"], context)
        assert result.success
        assert result.output == "HELLO"

    def test_write_mixed_case(self, functor, context):
        """Test write with mixed case input."""
        result = functor.format(["HeLLo WoRLd"], context)
        assert result.success
        assert result.output == "HELLO WORLD"

    def test_write_already_uppercase(self, functor, context):
        """Test write with already uppercase input."""
        result = functor.format(["HELLO"], context)
        assert result.success
        assert result.output == "HELLO"

    def test_write_numbers_and_symbols(self, functor, context):
        """Test write with numbers and symbols."""
        result = functor.format(["hello123!@#"], context)
        assert result.success
        assert result.output == "HELLO123!@#"

    def test_write_empty_input(self, functor, context):
        """Test write with empty input."""
        result = functor.format([], context)
        assert not result.success
        assert "No input value" in result.message

    def test_read_basic(self, functor, context):
        """Test basic read functionality."""
        outputs = _outputs_from_strings("result")
        result = functor.parse(["hello"], outputs, context)
        assert result.success
        assert result.outputs["result"].value == "HELLO"

    def test_read_multiple_outputs(self, functor, context):
        """Test read with multiple outputs."""
        outputs = _outputs_from_strings(["result1", "result2"])
        result = functor.parse(["hello"], outputs, context)
        assert result.success
        assert result.outputs["result1"].value == "HELLO"
        assert result.outputs["result2"].value == "HELLO"


class ConvertTestTitleCaseFunctor:
    """Test the TitleCase functor."""

    @pytest.fixture
    def functor(self):
        return ConvertTitleCaseFunctor()

    def test_write_basic(self, functor, context):
        """Test basic write functionality."""
        result = functor.format(["hello world"], context)
        assert result.success
        assert result.output == "Hello World"

    def test_write_mixed_case(self, functor, context):
        """Test write with mixed case input."""
        result = functor.format(["hELLo woRLd"], context)
        assert result.success
        assert result.output == "Hello World"

    def test_write_already_title_case(self, functor, context):
        """Test write with already title case input."""
        result = functor.format(["Hello World"], context)
        assert result.success
        assert result.output == "Hello World"

    def test_write_single_word(self, functor, context):
        """Test write with single word."""
        result = functor.format(["hello"], context)
        assert result.success
        assert result.output == "Hello"

    def test_write_with_apostrophes(self, functor, context):
        """Test write with apostrophes."""
        result = functor.format(["don't stop"], context)
        assert result.success
        assert result.output == "Don'T Stop"

    def test_write_empty_input(self, functor, context):
        """Test write with empty input."""
        result = functor.format([], context)
        assert not result.success
        assert "No input value" in result.message

    def test_read_basic(self, functor, context):
        """Test basic read functionality."""
        outputs = _outputs_from_strings("result")
        result = functor.parse(["hello world"], outputs, context)
        assert result.success
        assert result.outputs["result"].value == "Hello World"

    def test_read_multiple_outputs(self, functor, context):
        """Test read with multiple outputs."""
        outputs = _outputs_from_strings(["result1", "result2"])
        result = functor.parse(["hello world"], outputs, context)
        assert result.success
        assert result.outputs["result1"].value == "Hello World"
        assert result.outputs["result2"].value == "Hello World"
