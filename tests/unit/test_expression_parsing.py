# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""
Tests for expression parsing functionality.

This module tests the parsing of expression strings into SilexExpression
and SilexExpressionGraph data structures.
"""

import pytest

pytest.importorskip("silex.core", reason="silex C++ bindings not available")

from silex import SilexExpression, SilexExpressionGraph, FunctorOutput, SilexFunctorInfo, ExpressionInput
from silex import ExpressionMode, Language
from silex.core import ExpressionParser
from silex.core import Registry


@pytest.fixture
def registry():
    """Create a registry with test functors for expression parsing."""
    registry = Registry()

    # Register SplitCamelCaseFunctor
    registry.register_functor(
        SilexFunctorInfo(
            uid="silex.impl.functors.case_split.SplitCamelCaseFunctor",
            name="SplitCamelCaseFunctor",
            aliases=["CC", "camelcase"],
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

    # Register ConvertTitleCaseFunctor
    registry.register_functor(
        SilexFunctorInfo(
            uid="silex.impl.functors.case_conversion.ConvertTitleCaseFunctor",
            name="ConvertTitleCaseFunctor",
            aliases=["title"],
            module="silex.impl.functors.case_conversion",
            package="silex",
            language=Language.PYTHON,
        )
    )

    # Register GlobFunctor
    registry.register_functor(
        SilexFunctorInfo(
            uid="silex.impl.functors.glob.GlobFunctor",
            name="GlobFunctor",
            aliases=["glob"],
            module="silex.impl.functors.glob",
            package="silex",
            language=Language.PYTHON,
        )
    )

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

    return registry


@pytest.fixture
def expression_parser(registry):
    """Create an expression parser with the test registry."""
    return ExpressionParser(registry)


class TestExpressionParser:
    """Test the expression parser functionality."""

    def test_parser_initialization(self, registry):
        """Test that parser initializes correctly with registry."""
        parser = ExpressionParser(registry)
        assert parser._registry is registry

    def test_parse_simple_write_expression(self, expression_parser):
        """Test parsing a simple write expression."""
        expressions = ["$title({classification})"]
        graph = expression_parser.parse_expressions(expressions)

        assert isinstance(graph, SilexExpressionGraph)
        assert len(graph.expressions) == 1
        assert graph.mode == ExpressionMode.FORMAT

        expr = graph.expressions[0]

        # Check the basic structure with new behavior
        assert expr.raw == "$title({classification})"
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "{classification}"
        assert expr.inputs[0].value == "{classification}"  # New behavior: preserve format string
        assert expr.inputs[0].is_dynamic == True
        assert expr.outputs == []
        assert expr.functor_outputs == []
        assert expr.mode == ExpressionMode.FORMAT
        assert expr.warnings == []

        # Check that functor was resolved (title is an alias for ConvertTitleCaseFunctor)
        assert expr.functor_info is not None
        assert "title" in expr.functor_info.aliases

        # Check graph structure
        assert len(graph.inputs) == 1
        assert graph.inputs[0] == "classification"
        assert graph.outputs == ["output"]

    def test_parse_simple_read_expression(self, expression_parser):
        """Test parsing a simple read expression."""
        expressions = ["$lower({value})->classification"]
        graph = expression_parser.parse_expressions(expressions)

        assert len(graph.expressions) == 1
        assert graph.mode == ExpressionMode.PARSE

        expr = graph.expressions[0]

        # Check the basic structure
        assert expr.raw == "$lower({value})->classification"
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "{value}"
        assert expr.inputs[0].value == "{value}"  # New behavior: preserve format string
        assert expr.inputs[0].is_dynamic == True
        assert expr.outputs == ["classification"]
        assert len(expr.functor_outputs) == 1
        assert expr.functor_outputs[0].name == "classification"
        assert expr.functor_outputs[0].options == []
        assert expr.mode == ExpressionMode.PARSE
        assert expr.warnings == []

        # Check that functor was resolved (lower is an alias for ConvertLowerCaseFunctor)
        assert expr.functor_info is not None
        assert "lower" in expr.functor_info.aliases

        # Check graph structure
        assert len(graph.inputs) == 1
        assert graph.inputs[0] == "value"
        assert graph.outputs == ["classification"]

    def test_parse_expression_graph_connections(self, expression_parser):
        """Test parsing expressions with connections between them."""
        expressions = ["$CC({value})->classification,asset,variant", "$lower({classification})->final_classification"]
        graph = expression_parser.parse_expressions(expressions)

        assert len(graph.expressions) == 2
        assert graph.mode == ExpressionMode.PARSE

        # Check that classification output from first expression connects to input of second
        expected_connection = {(0, 0): (1, 0)}  # expr0 output0 -> expr1 input0
        assert graph.connections == expected_connection

        # Check graph inputs/outputs
        assert len(graph.inputs) == 1
        assert graph.inputs[0] == "value"
        assert set(graph.outputs) == {"classification", "asset", "variant", "final_classification"}

    def test_parse_write_read_combination(self, expression_parser):
        """Test combining write and read expressions."""
        expressions = [
            "$CC($L({classification}), {asset}, {variant})",  # Write with nested write
            "$lower({classification})->final_class",  # Read
        ]
        graph = expression_parser.parse_expressions(expressions)

        # With new nested parsing, we get 3 expressions: $L, $CC, $lower
        assert len(graph.expressions) == 3
        assert graph.mode == ExpressionMode.PARSE  # Parse mode because contains a parse expression

        # First expression: nested $L
        nested_expr = graph.expressions[0]
        assert nested_expr.raw == "$L({classification})"
        assert nested_expr.mode == ExpressionMode.FORMAT
        assert len(nested_expr.inputs) == 1
        assert nested_expr.inputs[0].value == "{classification}"

        # Second expression: main $CC with placeholder for nested
        main_expr = graph.expressions[1]
        assert main_expr.raw == "$CC($L({classification}), {asset}, {variant})"
        assert main_expr.mode == ExpressionMode.FORMAT
        assert len(main_expr.inputs) == 3
        assert main_expr.inputs[0].value == "{__nested_0__}"  # Placeholder for nested

        # Third expression: $lower read expression
        read_expr = graph.expressions[2]
        assert read_expr.raw == "$lower({classification})->final_class"
        assert read_expr.mode == ExpressionMode.PARSE
        assert read_expr.outputs == ["final_class"]

        # Check graph inputs - should include all variables not produced by expressions
        input_vars = set(graph.inputs)
        assert "classification" in input_vars
        assert "asset" in input_vars
        assert "variant" in input_vars

        # Check outputs - only outputs from read expressions (not write expressions)
        assert graph.outputs == ["final_class"]

    def test_parse_read_in_write_expression(self, expression_parser):
        """Test read expression nested inside write expression."""
        # This would be like: $title($lower({value})->temp)
        # But for now we'll test simpler case since nested parsing isn't implemented
        expressions = ["$lower({value})->temp", "$title({temp})"]
        graph = expression_parser.parse_expressions(expressions)

        assert len(graph.expressions) == 2
        assert graph.mode == ExpressionMode.PARSE

        # Check connection from temp output to temp input
        expected_connection = {(0, 0): (1, 0)}
        assert graph.connections == expected_connection

        # Graph inputs/outputs
        assert len(graph.inputs) == 1
        assert graph.inputs[0] == "value"
        assert set(graph.outputs) == {"temp"}  # Only non-underscore outputs

    def test_parse_multiple_connections(self, expression_parser):
        """Test complex graph with multiple connections."""
        expressions = [
            "$CC({value})->a,b,c",
            "$lower({a})->lower_a",
            "$title({b})->title_b",
            "$L({c}, {lower_a})->combined",
        ]
        graph = expression_parser.parse_expressions(expressions)

        assert len(graph.expressions) == 4

        # Check all connections
        expected_connections = {
            (0, 0): (1, 0),  # a -> lower input
            (0, 1): (2, 0),  # b -> title input
            (0, 2): (3, 0),  # c -> combined input
            (1, 0): (3, 1),  # lower_a -> combined input
        }
        assert graph.connections == expected_connections

        # Graph structure
        assert len(graph.inputs) == 1
        assert graph.inputs[0] == "value"
        assert set(graph.outputs) == {"a", "b", "c", "lower_a", "title_b", "combined"}

    def test_parse_graph_input_to_middle_expression(self, expression_parser):
        """Test graph input connecting to expression that isn't first."""
        expressions = [
            "$lower({first})->result1",
            "$title({second})->result2",  # second is graph input, not from first expr
            "$CC({result1}, {result2})->final",
        ]
        graph = expression_parser.parse_expressions(expressions)

        # Check graph inputs include both variables
        input_vars = set(graph.inputs)
        assert input_vars == {"first", "second"}

        # Check connections
        expected_connections = {
            (0, 0): (2, 0),  # result1 -> final
            (1, 0): (2, 1),  # result2 -> final
        }
        assert graph.connections == expected_connections

    def test_parse_read_expression_with_output_options(self, expression_parser):
        """Test parsing read expression with output options."""
        expressions = ["$CC({value})->classification:0,asset,variant:-1:?"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.mode == ExpressionMode.PARSE
        assert expr.functor_info.name == "SplitCamelCaseFunctor"

        # Check functor outputs with options
        assert len(expr.functor_outputs) == 3
        assert expr.functor_outputs[0].name == "classification"
        assert expr.functor_outputs[0].options == ["0"]
        assert expr.functor_outputs[1].name == "asset"
        assert expr.functor_outputs[1].options == []
        assert expr.functor_outputs[2].name == "variant"
        assert expr.functor_outputs[2].options == ["-1", "?"]

    def test_parse_multiple_expressions(self, expression_parser):
        """Test parsing multiple expressions into a graph."""
        expressions = ["$CC({value})->classification:0,asset,variant:-1:?", "$L({classification})->classification"]
        graph = expression_parser.parse_expressions(expressions)

        assert len(graph.expressions) == 2
        assert graph.expressions[0].functor_info.name == "SplitCamelCaseFunctor"
        assert graph.expressions[1].functor_info.name == "LexiconFunctor"

    def test_parse_expression_with_multiple_inputs(self, expression_parser):
        """Test parsing expression with multiple inputs."""
        expressions = ["$CC({asset}, {variant})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.inputs) == 2
        assert expr.inputs[0].raw == "{asset}"
        assert expr.inputs[1].raw == "{variant}"

    def test_parse_expression_with_string_literals(self, expression_parser):
        """Test parsing expression with quoted string literals."""
        expressions = ['$title("Hello World")']
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == '"Hello World"'
        # With new behavior, quoted strings are preserved (quotes not stripped for non-dynamic)
        assert expr.inputs[0].value == '"Hello World"'
        assert expr.inputs[0].is_dynamic == False

    def test_parse_expression_with_boolean_literals(self, expression_parser):
        """Test parsing expression with boolean literals."""
        expressions = ["$CC({input}, {True})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.inputs) == 2
        assert expr.inputs[1].raw == "{True}"
        assert expr.inputs[1].value is True

    def test_parse_expression_with_numeric_literals(self, expression_parser):
        """Test parsing expression with numeric literals."""
        expressions = ["$CC({input}, {42}, {3.14})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.inputs) == 3
        assert expr.inputs[1].raw == "{42}"
        assert expr.inputs[1].value == 42
        assert expr.inputs[2].raw == "{3.14}"
        assert expr.inputs[2].value == 3.14

    def test_parse_nested_expression(self, expression_parser):
        """Test parsing nested expressions."""
        expressions = ["$title($lower({classification}))"]
        graph = expression_parser.parse_expressions(expressions)

        # With new nested parsing, we get 2 separate expressions
        assert len(graph.expressions) == 2
        assert graph.mode == ExpressionMode.FORMAT

        # First expression: nested $lower
        nested_expr = graph.expressions[0]
        assert nested_expr.raw == "$lower({classification})"
        assert nested_expr.mode == ExpressionMode.FORMAT
        assert len(nested_expr.inputs) == 1
        assert nested_expr.inputs[0].value == "{classification}"

        # Second expression: main $title with placeholder
        main_expr = graph.expressions[1]
        assert main_expr.raw == "$title($lower({classification}))"
        assert main_expr.mode == ExpressionMode.FORMAT
        assert len(main_expr.inputs) == 1
        assert main_expr.inputs[0].value == "{__nested_0__}"  # Placeholder for nested

        # Graph structure
        assert len(graph.inputs) >= 1
        assert "classification" in graph.inputs
        assert graph.outputs == ["output"]

    def test_parse_expression_with_alias_resolution(self, expression_parser):
        """Test that functor aliases are properly resolved."""
        expressions = ["$L({value})->output"]  # L is alias for lower
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "LexiconFunctor"  # Should resolve to LexiconFunctor

    def test_parse_expression_with_package_prefix(self, expression_parser):
        """Test parsing expression with package-prefixed functor."""
        expressions = ["$silex.lower({value})->output"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "ConvertLowerCaseFunctor"

    def test_parse_unknown_functor_warning(self, expression_parser):
        """Test that unknown functors generate warnings."""
        expressions = ["$unknown_functor({value})->output"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.warnings) > 0
        assert any("unknown_functor" in warning for warning in expr.warnings)
        assert expr.functor_info is None

    def test_parse_malformed_expression(self, expression_parser):
        """Test parsing malformed expressions."""
        expressions = ["not an expression"]
        graph = expression_parser.parse_expressions(expressions)

        # Should handle gracefully, possibly with warnings
        assert isinstance(graph, SilexExpressionGraph)

    def test_parse_expression_with_complex_formatting(self, expression_parser):
        """Test parsing expression with complex string formatting."""
        expressions = [r"$glob({'FR*_'+{project.code}})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "GlobFunctor"
        assert len(expr.inputs) == 1
        # The complex formatting should be preserved in raw form
        assert "project.code" in expr.inputs[0].raw

    def test_parse_empty_expressions_list(self, expression_parser):
        """Test parsing empty list of expressions."""
        graph = expression_parser.parse_expressions([])

        assert isinstance(graph, SilexExpressionGraph)
        assert len(graph.expressions) == 0

    def test_parse_expression_with_spaces(self, expression_parser):
        """Test parsing expressions with various spacing."""
        expressions = ["$title( {classification} ) -> output", "$lower({value})  ->  result", "  $CC({value})  "]
        graph = expression_parser.parse_expressions(expressions)

        assert len(graph.expressions) == 3
        # Verify all expressions parsed correctly despite spacing
        for expr in graph.expressions:
            assert expr.functor_info is not None

    def test_parse_expression_with_quotes_and_commas(self, expression_parser):
        """Test parsing expressions with quoted strings containing commas."""
        expressions = ['$title("Hello, World", {other})']
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.inputs) == 2
        assert expr.inputs[0].raw == '"Hello, World"'
        # With new behavior, quoted strings are preserved
        assert expr.inputs[0].value == '"Hello, World"'
        assert expr.inputs[1].raw == "{other}"

    def test_parse_expression_with_nested_braces(self, expression_parser):
        """Test parsing expressions with nested braces in variables."""
        expressions = ["$lower({context.project.code})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "{context.project.code}"
        assert expr.inputs[0].is_dynamic is True

    def test_parse_real_world_file_resolver_examples(self, expression_parser):
        """Test parsing real examples from file_resolver.silex."""
        expressions = [
            "$CC($L({classification}), {asset}, {variant})",
            "$L({classification})->classification",
            "$title({tree})",
            "$glob({'FR*_'+{project.code}})",
            "$CC({value})->classification:0,asset,variant:-1:?",
        ]
        graph = expression_parser.parse_expressions(expressions)

        # With nested parsing, we get 6 expressions: $L, $CC, $L, $title, $glob, $CC
        assert len(graph.expressions) == 6

        # The nested $L from the first expression
        assert graph.expressions[0].raw == "$L({classification})"
        assert graph.expressions[0].mode == ExpressionMode.FORMAT

        # The main $CC from the first expression
        assert graph.expressions[1].raw == "$CC($L({classification}), {asset}, {variant})"
        assert graph.expressions[1].mode == ExpressionMode.FORMAT

        # The second expression $L (read mode)
        assert graph.expressions[2].raw == "$L({classification})->classification"
        assert graph.expressions[2].mode == ExpressionMode.PARSE

        # The title expression
        assert graph.expressions[3].raw == "$title({tree})"
        assert graph.expressions[3].mode == ExpressionMode.FORMAT

        # The glob expression with complex formatting
        assert graph.expressions[4].raw == "$glob({'FR*_'+{project.code}})"
        assert graph.expressions[4].mode == ExpressionMode.FORMAT

        # The final CC read expression
        assert graph.expressions[5].raw == "$CC({value})->classification:0,asset,variant:-1:?"
        assert graph.expressions[5].mode == ExpressionMode.PARSE

        # Check graph structure
        assert graph.mode == ExpressionMode.PARSE  # Contains parse expressions

        # Check that project.code is extracted from the complex formatting
        input_vars = set(graph.inputs)
        assert "project.code" in input_vars
        assert "tree" in input_vars
        assert "value" in input_vars

    def test_parse_expression_with_no_parentheses(self, expression_parser):
        """Test parsing functors called without parentheses."""
        expressions = ["$title"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "ConvertTitleCaseFunctor"
        assert len(expr.inputs) == 0

    def test_parse_expression_with_empty_arguments(self, expression_parser):
        """Test parsing expressions with empty arguments."""
        expressions = ["$title()"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "ConvertTitleCaseFunctor"
        assert len(expr.inputs) == 0

    def test_parse_complex_output_options(self, expression_parser):
        """Test parsing complex output option patterns."""
        expressions = ["$CC({value})->first:0:?:default,middle:1-3:+,last:-1:?:optional"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert len(expr.functor_outputs) == 3

        # Check first output
        assert expr.functor_outputs[0].name == "first"
        assert expr.functor_outputs[0].options == ["0", "?", "default"]

        # Check middle output
        assert expr.functor_outputs[1].name == "middle"
        assert expr.functor_outputs[1].options == ["1-3", "+"]

        # Check last output
        assert expr.functor_outputs[2].name == "last"
        assert expr.functor_outputs[2].options == ["-1", "?", "optional"]

    def test_parse_format_expression_simple(self, expression_parser):
        """Test parsing expressions with simple format strings."""
        expressions = ["$title(prefix_{variable}_suffix)"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "ConvertTitleCaseFunctor"
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "prefix_{variable}_suffix"
        assert expr.inputs[0].is_dynamic is True
        # Variable should be detected from the format string
        assert "variable" in str(expr.inputs[0].value)

    def test_parse_format_expression_multiple_variables(self, expression_parser):
        """Test parsing expressions with multiple variables in format strings."""
        expressions = ["$glob({project}_{asset}_{variant}.ext)"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "GlobFunctor"
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "{project}_{asset}_{variant}.ext"
        assert expr.inputs[0].is_dynamic is True

    def test_parse_format_expression_nested_variables(self, expression_parser):
        """Test parsing expressions with nested variable references."""
        expressions = ["$glob(FR*_{context.project})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "GlobFunctor"
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "FR*_{context.project}"
        assert expr.inputs[0].is_dynamic is True

    def test_parse_format_expression_with_quotes(self, expression_parser):
        """Test parsing expressions with quoted format strings."""
        expressions = ["$glob({'FR*_'+context.project})"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.functor_info.name == "GlobFunctor"
        assert len(expr.inputs) == 1
        # Should preserve the complex format expression
        assert "context.project" in expr.inputs[0].raw
        assert expr.inputs[0].is_dynamic is True

    def test_parse_format_expression_in_read_mode(self, expression_parser):
        """Test parsing format expressions in read mode."""
        expressions = ["$CC(prefix_{value}_suffix)->classification,asset,variant"]
        graph = expression_parser.parse_expressions(expressions)

        expr = graph.expressions[0]
        assert expr.mode == ExpressionMode.PARSE
        assert len(expr.inputs) == 1
        assert expr.inputs[0].raw == "prefix_{value}_suffix"
        assert expr.inputs[0].is_dynamic is True
        assert len(expr.outputs) == 3

    def test_parse_variable_overwriting_scenario(self, expression_parser):
        """Test parsing the variable overwriting scenario from the schema."""
        expressions = ["$CC({value})->classification:0,asset,variant:-1:?", "$L({classification})->classification"]
        graph = expression_parser.parse_expressions(expressions)

        # Should be read mode due to arrow
        assert graph.mode == ExpressionMode.PARSE
        assert len(graph.expressions) == 2

        # First expression outputs classification, asset, variant
        cc_expr = graph.expressions[0]
        assert cc_expr.mode == ExpressionMode.PARSE
        assert cc_expr.outputs == ["classification", "asset", "variant"]

        # Second expression takes classification input and outputs classification (overwrite)
        l_expr = graph.expressions[1]
        assert l_expr.mode == ExpressionMode.PARSE
        assert l_expr.outputs == ["classification"]

        # Graph should have connection from first classification to second
        # (0, 0) -> (1, 0) means expr 0 output 0 connects to expr 1 input 0
        assert (0, 0) in graph.connections
        assert graph.connections[(0, 0)] == (1, 0)

        # Graph inputs should only include "value" (classification is produced internally)
        assert graph.inputs == ["value"]

        # Graph outputs should include all unique outputs: asset, classification, variant
        # The classification from the second expression overwrites the first
        expected_outputs = {"asset", "classification", "variant"}
        assert set(graph.outputs) == expected_outputs

    def test_parse_multiple_variable_overwrites(self, expression_parser):
        """Test parsing multiple variable overwrites."""
        expressions = ["$CC({value})->a,b,c", "$L({a})->a", "$L({b})->b"]
        graph = expression_parser.parse_expressions(expressions)

        assert graph.mode == ExpressionMode.PARSE
        assert len(graph.expressions) == 3

        # Check connections
        assert (0, 0) in graph.connections  # a -> L(a)
        assert (0, 1) in graph.connections  # b -> L(b)
        assert graph.connections[(0, 0)] == (1, 0)
        assert graph.connections[(0, 1)] == (2, 0)

        # Graph should output a (overwritten), b (overwritten), c (original)
        expected_outputs = {"a", "b", "c"}
        assert set(graph.outputs) == expected_outputs

        # Only value is external input
        assert graph.inputs == ["value"]
