"""
Simple integration tests for expression evaluation functionality.

These tests use the actual Silex expression parser and evaluator with real functors
to test complete expression evaluation scenarios with basic setup.
"""

import pytest
from pathlib import Path

pytest.importorskip("silex.core", reason="silex C++ bindings not available")

from silex import Box

from silex.core import ExpressionParser
from silex.core import ExpressionEvaluator
from silex.core import Registry
from silex import FunctorContext, SilexConfig, SilexFunctorInfo
from silex import ExpressionMode, Language
from silex import set_verbosity


class TestExpressionEvaluatorIntegrationSimple:
    """Simple integration tests with real Silex functors."""

    @pytest.fixture(autouse=True)
    def setup_test_environment(self, monkeypatch):
        """Set up the test environment for each test."""
        test_resources = Path(__file__).parent.parent / "resources"
        monkeypatch.setenv("SILEX_SCHEMA_PATH", str(test_resources))

        # Enable basic logging for integration tests
        set_verbosity(1)

    @pytest.fixture
    def registry(self):
        """Create a registry with real functors registered."""
        registry = Registry()

        # Register LexiconFunctor
        registry.register_functor(
            SilexFunctorInfo(
                uid="silex.impl.functors.lexicon.LexiconFunctor",
                name="LexiconFunctor",
                aliases=["L"],
                module="silex.impl.functors.lexicon",
                package="silex",
                language=Language.PYTHON,
            )
        )

        # Register SplitCamelCaseFunctor
        registry.register_functor(
            SilexFunctorInfo(
                uid="silex.impl.functors.case_split.SplitCamelCaseFunctor",
                name="SplitCamelCaseFunctor",
                aliases=["CC"],
                module="silex.impl.functors.case_split",
                package="silex",
                language=Language.PYTHON,
            )
        )

        # Register ConvertLowerCaseFunctor
        registry.register_functor(
            SilexFunctorInfo(
                uid="silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                name="ConvertLowerCaseFunctor",
                aliases=["lower"],
                module="silex.impl.functors.case_conversion",
                package="silex",
                language=Language.PYTHON,
            )
        )

        return registry

    @pytest.fixture
    def expression_parser(self, registry):
        """Create an expression parser with real functors."""
        aliases = {
            "lower": "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
            "CC": "silex.impl.functors.case_split.SplitCamelCaseFunctor",
            "L": "silex.impl.functors.lexicon.LexiconFunctor",
        }
        return ExpressionParser(registry, aliases)

    @pytest.fixture
    def evaluator(self, registry):
        """Create an expression evaluator."""
        return ExpressionEvaluator(registry)

    @pytest.fixture
    def test_context(self):
        """Create a test context for expressions."""
        return FunctorContext(
            context=Box({"value": "TestValue", "group": ["part1", "part2", "part3"]}),
            parent="/test/path",
            segment="test_segment",
            variables=Box(
                {"lexicon": {"classification": {"character": ["chr", "char"], "prop": ["prp"], "environment": ["env"]}}}
            ),
        )

    @pytest.fixture
    def config_with_lexicon(self):
        """Create a config with lexicon data."""
        return SilexConfig(
            global_variables=Box(
                {"lexicon": {"classification": {"character": ["chr", "char"], "prop": ["prp"], "environment": ["env"]}}}
            ),
            functor_variables=Box({}),
        )

    def test_simple_lowercase_expression(self, expression_parser, evaluator, test_context, config_with_lexicon):
        """Test evaluating a simple lowercase expression."""
        # Parse the expression
        expressions = expression_parser.parse_expressions(["$lower({value})->result"])

        assert len(expressions.expressions) == 1
        expression = expressions.expressions[0]

        # Evaluate it
        inputs = [test_context.context["value"]]
        result = evaluator.evaluate_parse_expression(expression, inputs, test_context, config_with_lexicon)

        assert result.success
        assert "result" in result.outputs
        assert result.outputs["result"].value == "testvalue"

    def test_expression_parsing_structure(self, expression_parser):
        """Test that expressions parse correctly."""
        expressions = expression_parser.parse_expressions(["$lower({value})->result"])

        assert len(expressions.expressions) == 1
        assert expressions.mode == ExpressionMode.PARSE

        expression = expressions.expressions[0]
        assert expression.functor_info is not None
        assert expression.functor_info.uid == "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor"
        assert len(expression.inputs) == 1
        assert len(expression.outputs) == 1

    def test_basic_functor_instantiation(self, registry):
        """Test that functors can be instantiated from registry."""
        functor = registry.get_functor("silex.impl.functors.case_conversion.ConvertLowerCaseFunctor")

        # Basic test - should be able to instantiate without error
        assert functor is not None
