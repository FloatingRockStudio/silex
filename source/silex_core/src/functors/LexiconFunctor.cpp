/// @file LexiconFunctor.cpp
/// @brief Implementation of abbreviation/full-name mapping functor.

#include "LexiconFunctor.h"
#include "util/Logging.h"
#include "util/Utils.h"

#include <algorithm>
#include <variant>

namespace silex {
namespace functors {

namespace {

std::string inputToStr(const FunctorInput& input) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else return "";
    }, input);
}

using LexiconMap = std::map<std::string, std::string>;

/// Get the lexicon from functor context variables.
/// The lexicon structure is: category -> { full_name -> [abbreviations] }
/// We flatten this to: abbreviation -> full_name (for read/lookup).
LexiconMap getLexicon(const FunctorContext& context) {
    LexiconMap lexicon;

    auto it = context.variables.find("lexicon");
    if (it == context.variables.end()) return lexicon;

    // The lexicon is a nested ContextMap:
    // { "classification": { "character": ["chr", "character"], ... } }
    if (auto* categories = std::any_cast<ContextMap>(&it->second)) {
        for (const auto& [categoryName, categoryVal] : *categories) {
            if (auto* entries = std::any_cast<ContextMap>(&categoryVal)) {
                for (const auto& [fullName, abbrevVal] : *entries) {
                    // Each entry can be a vector<string> or vector<any> of abbreviations
                    if (auto* abbrevs = std::any_cast<std::vector<std::string>>(&abbrevVal)) {
                        for (const auto& abbrev : *abbrevs) {
                            std::string lowerAbbrev = abbrev;
                            std::transform(lowerAbbrev.begin(), lowerAbbrev.end(),
                                lowerAbbrev.begin(), ::tolower);
                            lexicon[lowerAbbrev] = fullName;
                        }
                    } else if (auto* anyVec = std::any_cast<std::vector<std::any>>(&abbrevVal)) {
                        for (const auto& item : *anyVec) {
                            if (auto* s = std::any_cast<std::string>(&item)) {
                                std::string lowerAbbrev = *s;
                                std::transform(lowerAbbrev.begin(), lowerAbbrev.end(),
                                    lowerAbbrev.begin(), ::tolower);
                                lexicon[lowerAbbrev] = fullName;
                            }
                        }
                    } else if (auto* s = std::any_cast<std::string>(&abbrevVal)) {
                        std::string lowerAbbrev = *s;
                        std::transform(lowerAbbrev.begin(), lowerAbbrev.end(),
                            lowerAbbrev.begin(), ::tolower);
                        lexicon[lowerAbbrev] = fullName;
                    }
                }
            } else if (auto* s = std::any_cast<std::string>(&categoryVal)) {
                // Simple flat key-value
                lexicon[categoryName] = *s;
            }
        }
    }

    return lexicon;
}

} // anonymous namespace

ParseResult LexiconFunctor::parse(
    const std::vector<FunctorInput>& inputs,
    const std::vector<FunctorOutput>& outputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return ParseResult{false, "No input value provided", {}};
    }

    if (outputs.empty()) {
        return ParseResult{false, "No output categories provided", {}};
    }

    std::string value = inputToStr(inputs[0]);
    std::string lowerValue = core::toLower(value);

    // Get raw lexicon structure for category-scoped lookups
    auto varIt = context.variables.find("lexicon");
    const ContextMap* categories = nullptr;
    if (varIt != context.variables.end()) {
        categories = std::any_cast<ContextMap>(&varIt->second);
    }

    // Also get flat lookup map
    auto lexicon = getLexicon(context);

    std::map<std::string, ResolvedValue> resultOutputs;

    for (const auto& output : outputs) {
        bool isOptional = false;
        for (const auto& opt : output.options) {
            if (opt == "?") { isOptional = true; break; }
        }

        bool found = false;
        std::string result;

        // Try category-scoped lookup using output name as category
        if (categories) {
            auto catIt = categories->find(output.name);
            if (catIt != categories->end()) {
                if (auto* entries = std::any_cast<ContextMap>(&catIt->second)) {
                    for (const auto& [fullName, abbrevVal] : *entries) {
                        // Check if input matches any abbreviation in this entry
                        if (auto* abbrevs = std::any_cast<std::vector<std::string>>(&abbrevVal)) {
                            for (const auto& abbrev : *abbrevs) {
                                if (core::toLower(abbrev) == lowerValue) {
                                    result = fullName;
                                    found = true;
                                    break;
                                }
                            }
                        } else if (auto* anyVec = std::any_cast<std::vector<std::any>>(&abbrevVal)) {
                            for (const auto& item : *anyVec) {
                                if (auto* s = std::any_cast<std::string>(&item)) {
                                    if (core::toLower(*s) == lowerValue) {
                                        result = fullName;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        } else if (auto* s = std::any_cast<std::string>(&abbrevVal)) {
                            if (core::toLower(*s) == lowerValue) {
                                result = fullName;
                                found = true;
                            }
                        }
                        if (found) break;
                    }
                }
            }
        }

        // Fall back to flat lexicon lookup
        if (!found) {
            auto it = lexicon.find(lowerValue);
            if (it != lexicon.end()) {
                result = it->second;
                found = true;
            }
        }

        if (found) {
            resultOutputs[output.name] = ResolvedValue{result, false};
        } else if (isOptional) {
            resultOutputs[output.name] = ResolvedValue{value, false};
        } else {
            return ParseResult{false, "No Lexicon parse value for: " + value, {}};
        }
    }

    return ParseResult{true, "Success", resultOutputs};
}

FormatResult LexiconFunctor::format(
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context) {

    if (inputs.empty()) {
        return FormatResult{false, "No input value provided", ""};
    }

    // Two-input form: $L(category, value) — category-scoped reverse lookup
    // One-input form: $L(value) — reverse lookup across all categories
    std::string category;
    std::string value;
    if (inputs.size() >= 2) {
        category = inputToStr(inputs[0]);
        value = inputToStr(inputs[1]);
    } else {
        value = inputToStr(inputs[0]);
    }

    std::string lowerValue = core::toLower(value);

    // Category-scoped reverse lookup from the raw lexicon structure
    if (!category.empty()) {
        auto varIt = context.variables.find("lexicon");
        if (varIt != context.variables.end()) {
            if (auto* categories = std::any_cast<ContextMap>(&varIt->second)) {
                auto catIt = categories->find(category);
                if (catIt != categories->end()) {
                    if (auto* entries = std::any_cast<ContextMap>(&catIt->second)) {
                        for (const auto& [fullName, abbrevVal] : *entries) {
                            if (core::toLower(fullName) == value) {
                                // Return first abbreviation
                                if (auto* abbrevs = std::any_cast<std::vector<std::string>>(&abbrevVal)) {
                                    if (!abbrevs->empty()) {
                                        return FormatResult{true, "Success", (*abbrevs)[0]};
                                    }
                                } else if (auto* anyVec = std::any_cast<std::vector<std::any>>(&abbrevVal)) {
                                    for (const auto& item : *anyVec) {
                                        if (auto* s = std::any_cast<std::string>(&item)) {
                                            return FormatResult{true, "Success", *s};
                                        }
                                    }
                                } else if (auto* s = std::any_cast<std::string>(&abbrevVal)) {
                                    return FormatResult{true, "Success", *s};
                                }
                            }
                        }
                    }
                    return FormatResult{false, "No Lexicon format value for: " + value, ""};
                }
            }
        }
        return FormatResult{false, "No lexicon category for " + category, ""};
    }

    // Fallback: flat reverse lookup across all categories
    auto lexicon = getLexicon(context);
    for (const auto& [abbrev, fullName] : lexicon) {
        if (core::toLower(fullName) == value) {
            return FormatResult{true, "Success", abbrev};
        }
    }

    return FormatResult{false, "No Lexicon format value for: " + value, ""};
}

} // namespace functors
} // namespace silex
