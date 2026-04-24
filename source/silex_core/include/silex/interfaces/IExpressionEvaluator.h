// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file IExpressionEvaluator.h
/// @brief Interface for evaluating expression graphs using registered functors.

#include <silex/structs.h>

#include <vector>

namespace silex {

/// Interface for evaluating expression graphs with dependency resolution.
class IExpressionEvaluator {
public:
    virtual ~IExpressionEvaluator() = default;

    /// Evaluate an entire expression graph using dependency resolution.
    virtual ParseResult evaluateGraph(
        const SilexExpressionGraph& graph,
        const FunctorContext& context,
        const SilexConfig& config) = 0;

    /// Evaluate a read expression (has arrow, produces multiple outputs).
    virtual ParseResult evaluateParseExpression(
        const SilexExpression& expression,
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context,
        const SilexConfig& config) = 0;

    /// Evaluate a write expression (no arrow, produces single output).
    virtual FormatResult evaluateFormatExpression(
        const SilexExpression& expression,
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context,
        const SilexConfig& config) = 0;
};

} // namespace silex
