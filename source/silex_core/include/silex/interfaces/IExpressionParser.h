// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file IExpressionParser.h
/// @brief Interface for parsing expression strings into evaluation graphs.

#include <silex/structs.h>

#include <string>
#include <vector>

namespace silex {

/// Interface for parsing expression strings into structured expressions.
class IExpressionParser {
public:
    virtual ~IExpressionParser() = default;

    /// Parse a list of expression strings into an expression graph.
    virtual SilexExpressionGraph parseExpressions(
        const std::vector<std::string>& expressions) = 0;
};

} // namespace silex
