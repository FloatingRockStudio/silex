/// @file CaseConversionFunctor.cpp
/// @brief Implementation of case conversion functors (lower, upper, title).

#include "CaseConversionFunctor.h"
#include "util/Utils.h"

#include <variant>

namespace silex {
namespace functors {

namespace {

std::string inputToString(const FunctorInput& input) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else return "";
    }, input);
}

} // anonymous namespace

// MARK: LowerCase

ParseResult ConvertLowerCaseFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No input value provided", {}};
    }

    std::string value = core::toLower(inputToString(inputs[0]));

    std::map<std::string, ResolvedValue> resultOutputs;
    for (const auto& output : outputs) {
        resultOutputs[output.name] = ResolvedValue{value, false};
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult ConvertLowerCaseFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No input values provided", ""};
    }

    return FormatResult{true, "Success", core::toLower(inputToString(inputs[0]))};
}

// MARK: UpperCase

ParseResult ConvertUpperCaseFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No input value provided", {}};
    }

    std::string value = core::toUpper(inputToString(inputs[0]));

    std::map<std::string, ResolvedValue> resultOutputs;
    for (const auto& output : outputs) {
        resultOutputs[output.name] = ResolvedValue{value, false};
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult ConvertUpperCaseFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No input values provided", ""};
    }

    return FormatResult{true, "Success", core::toUpper(inputToString(inputs[0]))};
}

// MARK: TitleCase

ParseResult ConvertTitleCaseFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No input value provided", {}};
    }

    std::string value = core::toTitle(inputToString(inputs[0]));

    std::map<std::string, ResolvedValue> resultOutputs;
    for (const auto& output : outputs) {
        resultOutputs[output.name] = ResolvedValue{value, false};
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult ConvertTitleCaseFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No input values provided", ""};
    }

    return FormatResult{true, "Success", core::toTitle(inputToString(inputs[0]))};
}

} // namespace functors
} // namespace silex
