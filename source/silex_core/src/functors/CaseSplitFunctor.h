// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file CaseSplitFunctor.h
/// @brief Functors for splitting/joining camelCase and snake_case strings.

#include <silex/interfaces/IFunctor.h>

#include <string>
#include <vector>

namespace silex {
namespace functors {

/// Abstract base for functors that split/combine strings by case convention.
class AbstractSplitPartsFunctor : public IFunctor {
public:
    /// Split a segment value into parts and join with separator on read.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Split a context value by separator and combine using case convention on write.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;

protected:
    /// Split a string into parts. Override in subclasses.
    virtual std::vector<std::string> splitStringToParts(const std::string& value) const = 0;
    /// Combine parts into a string. Override in subclasses.
    virtual std::string combinePartsToString(const std::vector<std::string>& parts) const = 0;
};

/// Splits camelCase strings. Aliases: camelcase, CC.
/// Example: "chrAlexBase" -> ["chr", "Alex", "Base"]
class SplitCamelCaseFunctor : public AbstractSplitPartsFunctor {
protected:
    /// Split a camelCase string into individual word parts.
    std::vector<std::string> splitStringToParts(const std::string& value) const override;
    /// Combine word parts into a camelCase string.
    std::string combinePartsToString(const std::vector<std::string>& parts) const override;
};

/// Splits snake_case strings. Aliases: snakecase, SC.
/// Example: "chr_alex_base" -> ["chr", "alex", "base"]
class SplitSnakeCaseFunctor : public AbstractSplitPartsFunctor {
protected:
    /// Split a snake_case string into individual word parts.
    std::vector<std::string> splitStringToParts(const std::string& value) const override;
    /// Combine word parts into a snake_case string.
    std::string combinePartsToString(const std::vector<std::string>& parts) const override;
};

} // namespace functors
} // namespace silex
