// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file IFunctor.h
/// @brief Interface for pluggable functor operations (parse/format string transformations).

#include <silex/structs.h>

#include <vector>

namespace silex {

/// Interface for functors that can be registered and executed.
class IFunctor {
public:
    virtual ~IFunctor() = default;

    /// Execute parse operation: parse input and produce named outputs.
    virtual ParseResult parse(
        const std::vector<FunctorInput>& inputs,
        const std::vector<FunctorOutput>& outputs,
        const FunctorContext& context) = 0;

    /// Execute format operation: take inputs and produce a single string output.
    virtual FormatResult format(
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context) = 0;
};

} // namespace silex
