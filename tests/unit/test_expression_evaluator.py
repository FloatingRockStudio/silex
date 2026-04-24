"""
Tests for expression evaluation functionality.

This module tests the evaluation of SilexExpression and SilexExpressionGraph
objects using registered functors and context data.
"""

import pytest

pytest.importorskip("silex.core", reason="silex C++ bindings not available")

from silex import Box

from silex import (
    SilexExpression,
    SilexExpressionGraph,
    FunctorContext,
    SilexFunctorInfo,
    SilexConfig,
    ExpressionInput,
    FunctorOutput,
    ParseResult,
    FormatResult,
    ResolvedValue,
)
from silex import ExpressionMode, Language
from silex.core import ExpressionEvaluator
from silex.core import Registry
from silex.interfaces import IFunctor
from silex import set_verbosity


# Test Functors - Simple implementations for unit testing
class TestLowerCaseFunctor(IFunctor):
    """Simple test functor that converts to lowercase."""

    def parse(self, inputs, outputs, context):
        """Read operation: parse lowercase string and extract outputs."""
        if not inputs:
            return ParseResult(success=False, message="No inputs provided")

        input_value = str(inputs[0])
        result_outputs = {}

        for output in outputs:
            if output.name == "value":
                result_outputs["value"] = ResolvedValue(value=input_value.lower())
            else:
                result_outputs[output.name] = ResolvedValue(value=input_value.lower())

        return ParseResult(success=True, message="Success", outputs=result_outputs)

    def format(self, inputs, context):
        """Write operation: convert inputs to lowercase string."""
        if not inputs:
            return FormatResult(success=False, message="No inputs provided", output="")

        input_value = str(inputs[0])
        return FormatResult(success=True, message="Success", output=input_value.lower())


class TestSplitFunctor(IFunctor):
    """Simple test functor that splits strings."""

    def parse(self, inputs, outputs, context):
        """Read operation: split string and assign to outputs."""
        if not inputs:
            return ParseResult(success=False, message="No inputs provided")

        input_value = str(inputs[0])
        parts = input_value.split("_")
        result_outputs = {}

        for i, output in enumerate(outputs):
            if i < len(parts):
                result_outputs[output.name] = ResolvedValue(value=parts[i])
            else:
                result_outputs[output.name] = ResolvedValue(value="")

        return ParseResult(success=True, message="Success", outputs=result_outputs)

    def format(self, inputs, context):
        """Write operation: join inputs with underscore."""
        if not inputs:
            return FormatResult(success=False, message="No inputs provided", output="")

        parts = [str(inp) for inp in inputs]
        return FormatResult(success=True, message="Success", output="_".join(parts))


class TestTitleCaseFunctor(IFunctor):
    """Simple test functor that converts to title case."""

    def parse(self, inputs, outputs, context):
        """Read operation: parse title case string."""
        if not inputs:
            return ParseResult(success=False, message="No inputs provided")

        input_value = str(inputs[0])
        result_outputs = {}

        for output in outputs:
            result_outputs[output.name] = ResolvedValue(value=input_value.title())

        return ParseResult(success=True, message="Success", outputs=result_outputs)

    def format(self, inputs, context):
        """Write operation: convert to title case."""
        if not inputs:
            return FormatResult(success=False, message="No inputs provided", output="")

        input_value = str(inputs[0])
        return FormatResult(success=True, message="Success", output=input_value.title())


@pytest.fixture
def test_functors():
    """Create test functor instances."""
    return {
        "lower": TestLowerCaseFunctor(),
        "split": TestSplitFunctor(),
        "title": TestTitleCaseFunctor(),
    }


@pytest.fixture
def registry(test_functors):
    """Create a registry with test functors."""
    registry = Registry()

    # Register test functors with instances
    registry.register_functor(
        SilexFunctorInfo(
            uid="test.functors.TestLowerCaseFunctor",
            name="TestLowerCaseFunctor",
            aliases=["lower", "L"],
            module="test.functors",
            package="test",
            language=Language.PYTHON,
        ),
        test_functors["lower"],
    )

    registry.register_functor(
        SilexFunctorInfo(
            uid="test.functors.TestSplitFunctor",
            name="TestSplitFunctor",
            aliases=["split", "S"],
            module="test.functors",
            package="test",
            language=Language.PYTHON,
        ),
        test_functors["split"],
    )

    registry.register_functor(
        SilexFunctorInfo(
            uid="test.functors.TestTitleCaseFunctor",
            name="TestTitleCaseFunctor",
            aliases=["title", "T"],
            module="test.functors",
            package="test",
            language=Language.PYTHON,
        ),
        test_functors["title"],
    )

    # Store functor instances for testing
    registry._test_functors = test_functors

    return registry


@pytest.fixture
def config():
    """Create a test configuration."""
    return SilexConfig(
        global_variables=Box(
            {
                "project": Box({"code": "TEST"}),
                "asset": "character",
                "variant": "main",
            }
        ),
        functor_variables={
            "test.functors.TestLowerCaseFunctor": Box({"mode": "strict"}),
            "test.functors.TestSplitFunctor": Box({"separator": "_"}),
        },
    )


@pytest.fixture
def context(config):
    """Create a test context."""
    return FunctorContext(
        context=Box({"scene": "intro"}),
        parent="/project/TEST",
        segment="character_main",
        variables=config.global_variables,
    )


@pytest.fixture
def expression_evaluator(registry):
    """Create an expression evaluator."""
    return ExpressionEvaluator(registry)


class TestExpressionEvaluator:
    """Test the expression evaluator functionality."""

    def test_evaluator_initialization(self, registry):
        """Test that evaluator initializes correctly."""
        evaluator = ExpressionEvaluator(registry)
        assert evaluator._registry is registry

    def test_evaluate_simple_write_expression(self, expression_evaluator, context, config):
        """Test evaluating a simple write expression."""
        # Create a simple write expression: $lower({classification})
        expression = SilexExpression(
            raw="$lower({classification})",
            inputs=[ExpressionInput(raw="{classification}", value="TestValue", is_dynamic=True)],
            outputs=[],
            functor_info=expression_evaluator._registry.get_functor_info("lower"),
            mode=ExpressionMode.FORMAT,
        )
        local_context = FunctorContext(
            context=Box({"classification": "TestValue"}),
            parent="/project/TEST",
            segment="character_main",
            variables=config.global_variables,
        )
        inputs = [local_context.context["classification"]]
        result = expression_evaluator.evaluate_format_expression(expression, inputs, local_context, config)
        assert isinstance(result, FormatResult)
        assert result.success is True
        assert result.message == "Success"
        assert result.output == "testvalue"

    def test_evaluate_simple_read_expression(self, expression_evaluator, context, config):
        """Test evaluating a simple read expression."""
        # Create a simple read expression: $split({value})->part1,part2
        expression = SilexExpression(
            raw="$split({value})->part1,part2",
            inputs=[ExpressionInput(raw="{value}", value="test_value", is_dynamic=True)],
            outputs=["part1", "part2"],
            functor_info=expression_evaluator._registry.get_functor_info("split"),
            functor_outputs=[FunctorOutput(name="part1", options=[]), FunctorOutput(name="part2", options=[])],
            mode=ExpressionMode.PARSE,
        )
        local_context = FunctorContext(
            context=Box({"value": "test_value"}),
            parent="/project/TEST",
            segment="character_main",
            variables=config.global_variables,
        )
        inputs = [local_context.context["value"]]
        result = expression_evaluator.evaluate_parse_expression(expression, inputs, local_context, config)
        assert isinstance(result, ParseResult)
        assert result.success is True
        assert result.message == "Success"
        assert len(result.outputs) == 2
        assert "part1" in result.outputs
        assert "part2" in result.outputs
        assert result.outputs["part1"].value == "test"
        assert result.outputs["part2"].value == "value"

    def test_evaluate_single_expression_graph(self, expression_evaluator, context, config):
        """Test evaluating a graph with a single expression."""
        # Create a graph with one expression
        expression = SilexExpression(
            raw="$lower({classification})",
            inputs=[ExpressionInput(raw="{classification}", value="classification", is_dynamic=True)],
            outputs=[],
            functor_info=expression_evaluator._registry.get_functor_info("lower"),
            mode=ExpressionMode.FORMAT,
        )
        graph = SilexExpressionGraph(
            expressions=[expression], inputs=["classification"], outputs=["output"], mode=ExpressionMode.FORMAT
        )
        local_context = FunctorContext(
            context=Box({"classification": "classification"}),
            parent="/project/TEST",
            segment="character_main",
            variables=config.global_variables,
        )
        result = expression_evaluator.evaluate_graph(graph, local_context, config)
        assert isinstance(result, ParseResult)
        assert result.success is True
        assert result.message == "Graph evaluation completed"
        assert isinstance(result.outputs, dict)

    def test_evaluate_connected_expressions_graph(self, expression_evaluator, context, config):
        """Test evaluating a graph with connected expressions."""
        # Create connected expressions: $split({value})->part1,part2 | $lower({part1})->result
        split_expr = SilexExpression(
            raw="$split({value})->part1,part2",
            inputs=[ExpressionInput(raw="{value}", value="value", is_dynamic=True)],
            outputs=["part1", "part2"],
            functor_info=expression_evaluator._registry.get_functor_info("split"),
            functor_outputs=[FunctorOutput(name="part1", options=[]), FunctorOutput(name="part2", options=[])],
            mode=ExpressionMode.PARSE,
        )
        lower_expr = SilexExpression(
            raw="$lower({part1})->result",
            inputs=[ExpressionInput(raw="{part1}", value="part1", is_dynamic=True)],
            outputs=["result"],
            functor_info=expression_evaluator._registry.get_functor_info("lower"),
            functor_outputs=[FunctorOutput(name="result", options=[])],
            mode=ExpressionMode.PARSE,
        )
        graph = SilexExpressionGraph(
            expressions=[split_expr, lower_expr],
            inputs=["value"],
            outputs=["part1", "part2", "result"],
            connections={(0, 0): (1, 0)},  # split part1 -> lower input
            mode=ExpressionMode.PARSE,
        )
        local_context = FunctorContext(
            context=Box({"value": "value"}),
            parent="/project/TEST",
            segment="character_main",
            variables=config.global_variables,
        )
        result = expression_evaluator.evaluate_graph(graph, local_context, config)
        assert isinstance(result, ParseResult)
        assert result.success is True
        assert result.message == "Graph evaluation completed"
        assert len(result.outputs) == 3
        assert "part1" in result.outputs
        assert "part2" in result.outputs
        assert "result" in result.outputs

    def test_evaluate_with_literal_inputs(self, expression_evaluator, context, config):
        """Test evaluating expressions with literal inputs."""
        # Create expression with literal: $lower("TestValue")
        expression = SilexExpression(
            raw='$lower("TestValue")',
            inputs=[ExpressionInput(raw='"TestValue"', value="TestValue", is_dynamic=False)],
            outputs=[],
            functor_info=expression_evaluator._registry.get_functor_info("lower"),
            mode=ExpressionMode.FORMAT,
        )
        local_context = FunctorContext(
            context=Box({}),
            parent="/project/TEST",
            segment="character_main",
            variables=config.global_variables,
        )
        inputs = ["TestValue"]
        result = expression_evaluator.evaluate_format_expression(expression, inputs, local_context, config)
        assert isinstance(result, FormatResult)
        assert result.success is True
        assert result.message == "Success"
        assert result.output == "testvalue"

    def test_evaluate_with_missing_functor(self, expression_evaluator, context, config):
        """Test evaluating expression with missing functor."""
        expression = SilexExpression(
            raw="$unknown({value})",
            inputs=[ExpressionInput(raw="{value}", value="test", is_dynamic=True)],
            outputs=[],
            functor_info=None,  # Missing functor
            mode=ExpressionMode.FORMAT,
        )
        local_context = FunctorContext(
            context=Box({"value": "test"}),
            parent="/project/TEST",
            segment="character_main",
            variables=config.global_variables,
        )
        inputs = [local_context.context["value"]]
        result = expression_evaluator.evaluate_format_expression(expression, inputs, local_context, config)
        assert isinstance(result, FormatResult)
        assert result.success is False
        assert result.output == ""
        assert "No functor information for expression: $unknown({value})" == result.message

    def test_evaluate_variable_overwriting_unit(self, expression_evaluator, context, config):
        """Test that variables can be overwritten and still appear in graph outputs (unit test)."""

        # Update config to include the input value
        test_config = SilexConfig(
            global_variables=Box(
                {
                    "project": Box({"code": "TEST"}),
                    "asset": "character",
                    "variant": "main",
                    "value": "test_value",  # Provide the input value
                }
            ),
            functor_variables=config.functor_variables,
        )

        # Test: $split({value})->classification,asset | $lower({classification})->classification
        # classification is both an output from split and input/output for lower
        expressions = [
            SilexExpression(
                raw="$split({value})->classification,asset",
                inputs=[ExpressionInput(raw="{value}", value="{value}", is_dynamic=True)],
                outputs=["classification", "asset"],
                functor_info=expression_evaluator._registry.get_functor_info("split"),
                functor_outputs=[
                    FunctorOutput(name="classification", options=[]),
                    FunctorOutput(name="asset", options=[]),
                ],
                mode=ExpressionMode.PARSE,
            ),
            SilexExpression(
                raw="$lower({classification})->classification",
                inputs=[ExpressionInput(raw="{classification}", value="{classification}", is_dynamic=True)],
                outputs=["classification"],
                functor_info=expression_evaluator._registry.get_functor_info("lower"),
                functor_outputs=[FunctorOutput(name="classification", options=[])],
                mode=ExpressionMode.PARSE,
            ),
        ]

        graph = SilexExpressionGraph(
            expressions=expressions,
            inputs=["value"],
            outputs=["classification", "asset"],  # classification appears in outputs even though overwritten
            connections={(0, 0): (1, 0)},  # classification output -> lower input
            mode=ExpressionMode.PARSE,
        )

        result = expression_evaluator.evaluate_graph(graph, context, test_config)

        assert isinstance(result, ParseResult)
        assert result.success is True
        assert result.message == "Graph evaluation completed"
        assert len(result.outputs) == 2
        assert "classification" in result.outputs  # Should be the final lower-processed value
        assert "asset" in result.outputs

        # Verify the values - classification should be lowercased from split result
        assert result.outputs["classification"].value == "test"  # split result "test_value" -> "test", then lowercased
        assert result.outputs["asset"].value == "value"  # split result

    def test_evaluate_multiple_variable_overwrites_unit(self, expression_evaluator, context, config):
        """Test multiple variable overwrites in a single graph (unit test)."""

        # Update config to include the input value
        test_config = SilexConfig(
            global_variables=Box(),
            functor_variables=config.functor_variables,
        )

        # Test: $split({value})->a,b | $lower({a})->a | $title({b})->b
        expressions = [
            SilexExpression(
                raw="$split({value})->a,b",
                inputs=[ExpressionInput(raw="{value}", value="{value}", is_dynamic=True)],
                outputs=["a", "b"],
                functor_info=expression_evaluator._registry.get_functor_info("split"),
                functor_outputs=[FunctorOutput(name="a", options=[]), FunctorOutput(name="b", options=[])],
                mode=ExpressionMode.PARSE,
            ),
            SilexExpression(
                raw="$lower({a})->a",
                inputs=[ExpressionInput(raw="{a}", value="{a}", is_dynamic=True)],
                outputs=["a"],
                functor_info=expression_evaluator._registry.get_functor_info("lower"),
                functor_outputs=[FunctorOutput(name="a", options=[])],
                mode=ExpressionMode.PARSE,
            ),
            SilexExpression(
                raw="$title({b})->b",
                inputs=[ExpressionInput(raw="{b}", value="{b}", is_dynamic=True)],
                outputs=["b"],
                functor_info=expression_evaluator._registry.get_functor_info("title"),
                functor_outputs=[FunctorOutput(name="b", options=[])],
                mode=ExpressionMode.PARSE,
            ),
        ]

        graph = SilexExpressionGraph(
            expressions=expressions,
            inputs=["value"],
            outputs=["a", "b"],  # Both variables appear in outputs despite overwrites
            connections={
                (0, 0): (1, 0),  # a -> lower(a)
                (0, 1): (2, 0),  # b -> title(b)
            },
            mode=ExpressionMode.PARSE,
        )
        local_context = FunctorContext(
            context=Box({"value": "hello_world"}),
            parent="/project/TEST",
            segment="character_main",
            variables=test_config.global_variables,
        )
        result = expression_evaluator.evaluate_graph(graph, local_context, test_config)
        assert isinstance(result, ParseResult)
        assert result.success is True
        assert result.message == "Graph evaluation completed"
        assert len(result.outputs) == 2
        assert "a" in result.outputs  # Should be lower-processed value
        assert "b" in result.outputs  # Should be title-processed value

        # Verify the values are properly processed
        assert result.outputs["a"].value == "hello"  # split result "hello", then lowercased -> "hello"
        assert result.outputs["b"].value == "World"  # split result "world", then title -> "World"
