#pragma once

/// @file CaseConversionFunctor.h
/// @brief Functors for lowercase, uppercase, and title case string conversion.

#include <silex/interfaces/IFunctor.h>

namespace silex {
namespace functors {

/// Converts input to lowercase. Aliases: lower_case, lowercase, lower.
class ConvertLowerCaseFunctor : public IFunctor {
public:
    /// Convert segment value to lower case on read.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Convert context value to lower case on write.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;
};

/// Converts input to uppercase. Aliases: upper_case, uppercase.
class ConvertUpperCaseFunctor : public IFunctor {
public:
    /// Convert segment value to upper case on read.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Convert context value to upper case on write.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;
};

/// Converts input to title case. Aliases: title_case, titlecase, title.
class ConvertTitleCaseFunctor : public IFunctor {
public:
    /// Convert segment value to title case on read.
    ParseResult parse(const std::vector<FunctorInput>& inputs,
                    const std::vector<FunctorOutput>& outputs,
                    const FunctorContext& context) override;
    /// Convert context value to title case on write.
    FormatResult format(const std::vector<FunctorInput>& inputs,
                      const FunctorContext& context) override;
};

} // namespace functors
} // namespace silex
