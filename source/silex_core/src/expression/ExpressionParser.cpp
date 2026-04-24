// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file ExpressionParser.cpp
/// @brief Implementation of functor expression parsing into DAG structures.

#include "ExpressionParser.h"
#include "util/Logging.h"
#include "registry/Registry.h"
#include "util/Utils.h"

#include <algorithm>
#include <regex>
#include <set>
#include <sstream>

namespace silex {
namespace core {

// MARK: Public

ExpressionParser::ExpressionParser(
    Registry& registry,
    const std::map<std::string, std::string>& schemaAliases)
    : m_registry(registry), m_schemaAliases(schemaAliases) {}

SilexExpressionGraph ExpressionParser::parseExpressions(
    const std::vector<std::string>& expressions) {

    auto logger = getLogger(LoggerNames::ExpressionParser);
    logger->debug("Processing {} expression strings", expressions.size());

    std::vector<SilexExpression> allExpressions;

    for (size_t i = 0; i < expressions.size(); ++i) {
        const auto& exprStr = expressions[i];
        if (exprStr.empty()) continue;

        auto parsed = parseExpressionString(exprStr);
        allExpressions.insert(allExpressions.end(), parsed.begin(), parsed.end());
    }

    // Determine graph mode
    ExpressionMode graphMode = ExpressionMode::Format;
    for (const auto& expr : allExpressions) {
        if (expr.mode == ExpressionMode::Parse) {
            graphMode = ExpressionMode::Parse;
            break;
        }
    }

    // Build connections
    auto gc = buildGraphConnections(allExpressions, graphMode);

    return SilexExpressionGraph{
        allExpressions,
        gc.inputs,
        gc.outputs,
        gc.connections,
        graphMode
    };
}

// MARK: Private

std::vector<SilexExpression> ExpressionParser::parseExpressionString(
    const std::string& exprStr) {

    auto logger = getLogger(LoggerNames::ExpressionParser);

    // Check for arrow to determine mode
    std::smatch arrowMatch;
    bool hasArrow = std::regex_search(exprStr, arrowMatch, ExpressionPatterns::ARROW_OPERATOR);
    ExpressionMode mode = hasArrow ? ExpressionMode::Parse : ExpressionMode::Format;

    std::vector<SilexExpression> parsed;
    auto mainExpr = parseMainExpression(exprStr, mode, parsed);

    if (mainExpr) {
        parsed.push_back(*mainExpr);
    }

    return parsed;
}

std::optional<SilexExpression> ExpressionParser::parseMainExpression(
    const std::string& exprStr,
    ExpressionMode mode,
    std::vector<SilexExpression>& nestedExpressions) {

    auto logger = getLogger(LoggerNames::ExpressionParser);
    std::string functorPart;
    std::string outputsPart;

    if (mode == ExpressionMode::Parse) {
        std::smatch arrowMatch;
        if (std::regex_search(exprStr, arrowMatch, ExpressionPatterns::ARROW_OPERATOR)) {
            functorPart = exprStr.substr(0, arrowMatch.position());
            outputsPart = exprStr.substr(arrowMatch.position() + arrowMatch.length());
            // Trim
            while (!functorPart.empty() && functorPart.back() == ' ') functorPart.pop_back();
            while (!outputsPart.empty() && outputsPart.front() == ' ') outputsPart.erase(outputsPart.begin());
        } else {
            functorPart = exprStr;
        }
    } else {
        functorPart = exprStr;
    }

    // For WRITE mode, detect template expressions (functor embedded in string)
    std::string templatePrefix;
    std::string templateSuffix;
    std::string mainFunctorCall = functorPart;

    if (mode == ExpressionMode::Format) {
        std::smatch functorMatch;
        if (std::regex_search(functorPart, functorMatch, ExpressionPatterns::FUNCTOR_CALL)) {
            templatePrefix = functorPart.substr(0, functorMatch.position());
            mainFunctorCall = functorMatch[0].str();
            templateSuffix = functorPart.substr(functorMatch.position() + functorMatch.length());
        } else {
            // No functor found
            SilexExpression expr;
            expr.raw = exprStr;
            expr.mode = mode;
            expr.warnings.push_back("No functor found in write expression: " + exprStr);
            return expr;
        }
    }

    // Parse the main functor call
    std::smatch mainMatch;
    if (!std::regex_match(mainFunctorCall, mainMatch, ExpressionPatterns::FUNCTOR_CALL)) {
        // Not a functor call — might be a simple literal->variable assignment
        // e.g. "assets->tree" or "{value}->tree"
        if (mode == ExpressionMode::Parse && !outputsPart.empty()) {
            SilexExpression expr;
            expr.raw = exprStr;
            expr.mode = mode;
            // Parse the literal/variable input
            auto inputs = parseInputs(mainFunctorCall);
            expr.inputs = inputs;
            // Parse outputs
            auto [outs, fOuts] = parseOutputs(outputsPart);
            expr.outputs = outs;
            expr.functorOutputs = fOuts;
            // Use identity functor (passthrough)
            expr.functorName = "__identity__";
            return expr;
        }
        SilexExpression expr;
        expr.raw = exprStr;
        expr.mode = mode;
        expr.warnings.push_back("Invalid expression format: " + exprStr);
        return expr;
    }

    std::string functorName = mainMatch[1].str();
    std::string argsStr = mainMatch[2].matched ? mainMatch[2].str() : "";

    // Extract nested expressions from arguments
    auto [processedArgs, nestedCalls] = extractNestedFunctors(argsStr);

    // Parse nested functor calls
    for (const auto& nestedCall : nestedCalls) {
        auto nestedExpr = parseNestedFunctor(nestedCall, mode);
        if (nestedExpr) {
            nestedExpressions.push_back(*nestedExpr);
        }
    }

    // Resolve functor info
    auto functorInfo = resolveFunctorInfo(functorName);
    std::vector<std::string> warnings;
    if (!functorInfo) {
        warnings.push_back("Unknown functor: " + functorName);
    }

    // Parse inputs
    auto inputs = parseInputs(processedArgs);

    // Parse outputs for read expressions
    std::vector<std::string> outputs;
    std::vector<FunctorOutput> functorOutputs;
    if (mode == ExpressionMode::Parse && !outputsPart.empty()) {
        auto [outs, fOuts] = parseOutputs(outputsPart);
        outputs = outs;
        functorOutputs = fOuts;
    }

    // Handle template expressions
    if (mode == ExpressionMode::Format && (!templatePrefix.empty() || !templateSuffix.empty())) {
        std::string templateFormat = templatePrefix + "{_output}" + templateSuffix;

        // Extract template variables
        auto templateVars = extractVariableNames(templatePrefix);
        auto suffixVars = extractVariableNames(templateSuffix);
        templateVars.insert(suffixVars.begin(), suffixVars.end());

        for (const auto& varName : templateVars) {
            if (varName != "_output") {
                ExpressionInput inp;
                inp.raw = "{" + varName + "}";
                inp.value = "{" + varName + "}";
                inp.isDynamic = true;
                inputs.push_back(inp);
            }
        }

        std::string templateRaw = "TEMPLATE:" + templateFormat + "|FUNCTOR:" + mainFunctorCall;

        SilexExpression expr;
        expr.raw = templateRaw;
        expr.inputs = inputs;
        expr.outputs = outputs;
        expr.functorInfo = functorInfo;
        expr.functorOutputs = functorOutputs;
        expr.mode = mode;
        expr.warnings = warnings;
        return expr;
    }

    SilexExpression expr;
    expr.raw = exprStr;
    expr.functorName = functorName;
    expr.inputs = inputs;
    expr.outputs = outputs;
    expr.functorInfo = functorInfo;
    expr.functorOutputs = functorOutputs;
    expr.mode = mode;
    expr.warnings = warnings;
    return expr;
}

std::pair<std::string, std::vector<std::string>> ExpressionParser::extractNestedFunctors(
    const std::string& argsStr) {

    if (argsStr.empty()) return {"", {}};

    std::vector<std::string> nestedCalls;
    std::string processed = argsStr;

    // Find all nested functor calls
    std::vector<std::smatch> matches;
    std::string::const_iterator searchStart = argsStr.cbegin();
    std::smatch match;
    while (std::regex_search(searchStart, argsStr.cend(), match, ExpressionPatterns::FUNCTOR_CALL)) {
        matches.push_back(match);
        searchStart = match.suffix().first;
    }

    // Process in reverse to maintain positions
    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        std::string call = (*it)[0].str();
        nestedCalls.insert(nestedCalls.begin(), call);

        size_t pos = it->position();
        size_t len = it->length();
        std::string placeholder = "{__nested_" + std::to_string(nestedCalls.size() - 1) + "__}";
        processed = processed.substr(0, pos) + placeholder + processed.substr(pos + len);
    }

    return {processed, nestedCalls};
}

std::optional<SilexExpression> ExpressionParser::parseNestedFunctor(
    const std::string& functorCall, ExpressionMode parentMode) {

    std::smatch match;
    if (!std::regex_match(functorCall, match, ExpressionPatterns::FUNCTOR_CALL)) {
        return std::nullopt;
    }

    std::string name = match[1].str();
    std::string argsStr = match[2].matched ? match[2].str() : "";

    auto info = resolveFunctorInfo(name);
    std::vector<std::string> warnings;
    if (!info) {
        warnings.push_back("Unknown functor: " + name);
    }

    auto inputs = parseInputs(argsStr);

    std::vector<std::string> outputs;
    std::vector<FunctorOutput> functorOutputs;
    if (parentMode == ExpressionMode::Parse) {
        std::string tempName = "__temp_" + std::to_string(inputs.size()) + "__";
        outputs.push_back(tempName);
        functorOutputs.push_back(FunctorOutput{tempName, {}});
    }

    SilexExpression expr;
    expr.raw = functorCall;
    expr.inputs = inputs;
    expr.outputs = outputs;
    expr.functorInfo = info;
    expr.functorOutputs = functorOutputs;
    expr.mode = parentMode;
    expr.warnings = warnings;
    return expr;
}

ExpressionParser::GraphConnections ExpressionParser::buildGraphConnections(
    const std::vector<SilexExpression>& expressions, ExpressionMode graphMode) {

    GraphConnections gc;
    std::set<std::string> allOutputs;
    std::set<std::string> allInputs;

    // Collect output producers
    std::map<std::string, std::pair<int, int>> outputProducers;
    for (size_t i = 0; i < expressions.size(); ++i) {
        const auto& expr = expressions[i];
        if (expr.mode == ExpressionMode::Parse) {
            for (size_t j = 0; j < expr.outputs.size(); ++j) {
                allOutputs.insert(expr.outputs[j]);
                outputProducers[expr.outputs[j]] = {static_cast<int>(i), static_cast<int>(j)};
            }
        } else {
            allOutputs.insert("_output");
            outputProducers["_output"] = {static_cast<int>(i), 0};
        }
    }

    // Collect inputs and build connections
    for (size_t exprIdx = 0; exprIdx < expressions.size(); ++exprIdx) {
        const auto& expr = expressions[exprIdx];
        for (size_t inputIdx = 0; inputIdx < expr.inputs.size(); ++inputIdx) {
            auto varNames = extractVariableNames(expr.inputs[inputIdx].raw);
            for (const auto& varName : varNames) {
                allInputs.insert(varName);

                // Handle nested functor placeholders
                if (varName.substr(0, 9) == "__nested_" &&
                    varName.substr(varName.size() - 2) == "__") {
                    try {
                        int nestedIdx = std::stoi(varName.substr(9, varName.size() - 11));
                        if (nestedIdx < static_cast<int>(exprIdx)) {
                            gc.connections[{nestedIdx, 0}] = {
                                static_cast<int>(exprIdx), static_cast<int>(inputIdx)};
                        }
                    } catch (...) {}
                    continue;
                }

                // Connect to previous expression outputs
                for (size_t srcIdx = 0; srcIdx < exprIdx; ++srcIdx) {
                    const auto& srcExpr = expressions[srcIdx];
                    if (srcExpr.mode == ExpressionMode::Parse) {
                        for (size_t srcOut = 0; srcOut < srcExpr.outputs.size(); ++srcOut) {
                            if (varName == srcExpr.outputs[srcOut]) {
                                gc.connections[{static_cast<int>(srcIdx), static_cast<int>(srcOut)}] =
                                    {static_cast<int>(exprIdx), static_cast<int>(inputIdx)};
                                break;
                            }
                        }
                    } else if (varName == "_output") {
                        gc.connections[{static_cast<int>(srcIdx), 0}] =
                            {static_cast<int>(exprIdx), static_cast<int>(inputIdx)};
                    }
                }
            }
        }
    }

    // Graph inputs = variables not produced by any expression
    for (const auto& inp : allInputs) {
        if (allOutputs.find(inp) == allOutputs.end()) {
            gc.inputs.push_back(inp);
        }
    }
    std::sort(gc.inputs.begin(), gc.inputs.end());

    // Graph outputs
    if (graphMode == ExpressionMode::Parse) {
        std::set<std::string> finalOutputs;
        for (const auto& expr : expressions) {
            for (const auto& out : expr.outputs) {
                if (out.empty() || out[0] != '_') {
                    finalOutputs.insert(out);
                }
            }
        }
        gc.outputs.assign(finalOutputs.begin(), finalOutputs.end());
    } else {
        gc.outputs = {"output"};
    }

    return gc;
}

std::optional<SilexFunctorInfo> ExpressionParser::resolveFunctorInfo(
    const std::string& name) {

    // Check schema aliases first
    auto aliasIt = m_schemaAliases.find(name);
    if (aliasIt != m_schemaAliases.end()) {
        return m_registry.getFunctorInfo(aliasIt->second);
    }

    return m_registry.getFunctorInfo(name);
}

std::vector<ExpressionInput> ExpressionParser::parseInputs(
    const std::string& argsStr) {

    if (argsStr.empty()) return {};

    std::vector<ExpressionInput> inputs;
    auto args = splitArguments(argsStr);

    for (const auto& arg : args) {
        std::string trimmed = arg;
        // Trim whitespace
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (trimmed.empty()) continue;

        std::string stripped = stripQuotes(trimmed);

        // Nested functor placeholder
        if (trimmed.size() > 12 &&
            trimmed.substr(0, 10) == "{__nested_" &&
            trimmed.substr(trimmed.size() - 3) == "__}") {
            ExpressionInput inp;
            inp.raw = trimmed;
            inp.value = trimmed;
            inp.isDynamic = true;
            inputs.push_back(inp);
            continue;
        }

        bool literal = isLiteralValue(stripped);
        bool isDynamic = true;
        FunctorInput value = stripped;

        if (trimmed.find('{') == std::string::npos && !literal) {
            value = trimmed;
            isDynamic = false;
        } else if (literal) {
            value = parseLiteralValue(stripped);
            isDynamic = false;
        } else {
            auto varNames = extractVariableNames(trimmed);
            if (!varNames.empty()) {
                value = trimmed;
            } else {
                value = stripped;
                isDynamic = false;
            }
        }

        ExpressionInput inp;
        inp.raw = trimmed;
        inp.value = value;
        inp.isDynamic = isDynamic;
        inputs.push_back(inp);
    }

    return inputs;
}

std::pair<std::vector<std::string>, std::vector<FunctorOutput>>
ExpressionParser::parseOutputs(const std::string& outputsStr) {

    std::vector<std::string> outputs;
    std::vector<FunctorOutput> functorOutputs;

    auto specs = splitArguments(outputsStr);
    for (const auto& spec : specs) {
        std::string trimmed = spec;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (trimmed.empty()) continue;

        auto parts = splitString(trimmed, ':');
        std::string name = parts[0];
        // Trim name
        while (!name.empty() && name.front() == ' ') name.erase(name.begin());
        while (!name.empty() && name.back() == ' ') name.pop_back();

        std::vector<std::string> options;
        for (size_t i = 1; i < parts.size(); ++i) {
            std::string opt = parts[i];
            while (!opt.empty() && opt.front() == ' ') opt.erase(opt.begin());
            while (!opt.empty() && opt.back() == ' ') opt.pop_back();
            if (!opt.empty()) options.push_back(opt);
        }

        outputs.push_back(name);
        functorOutputs.push_back(FunctorOutput{name, options});
    }

    return {outputs, functorOutputs};
}

std::vector<std::string> ExpressionParser::splitArguments(
    const std::string& argsStr) {

    if (argsStr.empty()) return {};

    std::vector<std::string> args;
    std::string current;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuotes = false;
    char quoteChar = 0;

    for (char c : argsStr) {
        if ((c == '"' || c == '\'') && !inQuotes) {
            inQuotes = true;
            quoteChar = c;
        } else if (c == quoteChar && inQuotes) {
            inQuotes = false;
            quoteChar = 0;
        } else if (!inQuotes) {
            if (c == '(') parenDepth++;
            else if (c == ')') parenDepth--;
            else if (c == '{') braceDepth++;
            else if (c == '}') braceDepth--;
            else if (c == ',' && parenDepth == 0 && braceDepth == 0) {
                // Trim and add
                std::string trimmed = current;
                while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                args.push_back(trimmed);
                current.clear();
                continue;
            }
        }
        current += c;
    }

    if (!current.empty()) {
        std::string trimmed = current;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (!trimmed.empty()) args.push_back(trimmed);
    }

    return args;
}

std::string ExpressionParser::stripQuotes(const std::string& text) {
    if (text.size() >= 2) {
        if ((text.front() == '"' && text.back() == '"') ||
            (text.front() == '\'' && text.back() == '\'')) {
            return text.substr(1, text.size() - 2);
        }
    }
    return text;
}

std::set<std::string> ExpressionParser::extractVariableNames(
    const std::string& text) {

    std::set<std::string> names;

    // Simple brace extraction
    size_t pos = 0;
    while (pos < text.size()) {
        if (text[pos] == '{') {
            int depth = 1;
            size_t start = pos + 1;
            size_t j = start;
            while (j < text.size() && depth > 0) {
                if (text[j] == '{') depth++;
                else if (text[j] == '}') depth--;
                j++;
            }
            if (depth == 0) {
                std::string content = text.substr(start, j - start - 1);
                if (isSimpleVariable(content)) {
                    if (!content.empty()) {
                        // Strip array/function access
                        auto bracketPos = content.find_first_of("[(");
                        if (bracketPos != std::string::npos) {
                            content = content.substr(0, bracketPos);
                        }
                        names.insert(content);
                    }
                } else {
                    auto nested = extractVariablesWithNestedBraces(content);
                    names.insert(nested.begin(), nested.end());
                }
                pos = j;
            } else {
                pos++;
            }
        } else {
            pos++;
        }
    }

    return names;
}

std::set<std::string> ExpressionParser::extractVariablesWithNestedBraces(
    const std::string& text) {

    std::set<std::string> names;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '{') {
            int depth = 1;
            size_t j = i + 1;
            while (j < text.size() && depth > 0) {
                if (text[j] == '{') depth++;
                else if (text[j] == '}') depth--;
                j++;
            }
            if (depth == 0) {
                std::string content = text.substr(i + 1, j - i - 2);
                if (isSimpleVariable(content)) {
                    names.insert(content);
                } else {
                    auto nested = extractVariablesWithNestedBraces(content);
                    names.insert(nested.begin(), nested.end());
                }
                i = j;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }

    // If no nested braces found, try to extract variables from format expressions
    // (e.g., 'FR*_'+context.project → context.project is a variable)
    if (names.empty() && !text.empty()) {
        static const std::regex varTokenPattern(R"([a-zA-Z_][a-zA-Z0-9_.]*[a-zA-Z0-9_])");
        std::sregex_iterator it(text.begin(), text.end(), varTokenPattern);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            std::string token = (*it).str();
            // Skip string literals inside quotes and known keywords
            size_t matchPos = static_cast<size_t>(it->position());
            if (matchPos > 0 && (text[matchPos - 1] == '\'' || text[matchPos - 1] == '"')) continue;
            if (token.find('.') != std::string::npos) {
                names.insert(token);
            }
        }
    }

    return names;
}

bool ExpressionParser::isSimpleVariable(const std::string& content) {
    static const std::regex varPattern(R"(^[a-zA-Z_][a-zA-Z0-9_.\[\]]*$)");
    std::string trimmed = content;
    while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
    return std::regex_match(trimmed, varPattern);
}

bool ExpressionParser::isLiteralValue(const std::string& text) {
    if (text.size() < 2 || text.front() != '{' || text.back() != '}') {
        return false;
    }
    std::string inner = text.substr(1, text.size() - 2);
    // Trim
    while (!inner.empty() && inner.front() == ' ') inner.erase(inner.begin());
    while (!inner.empty() && inner.back() == ' ') inner.pop_back();

    std::string lower = toLower(inner);
    if (lower == "true" || lower == "false") return true;

    // Check numeric
    try {
        std::stod(inner);
        return true;
    } catch (...) {}

    return false;
}

FunctorInput ExpressionParser::parseLiteralValue(const std::string& text) {
    if (text.size() >= 2 && text.front() == '{' && text.back() == '}') {
        std::string inner = text.substr(1, text.size() - 2);
        while (!inner.empty() && inner.front() == ' ') inner.erase(inner.begin());
        while (!inner.empty() && inner.back() == ' ') inner.pop_back();

        std::string lower = toLower(inner);
        if (lower == "true") return true;
        if (lower == "false") return false;

        // Try integer
        if (inner.find('.') == std::string::npos) {
            try { return std::stoi(inner); } catch (...) {}
        }

        // Try double
        try {
            double d = std::stod(inner);
            return d;
        } catch (...) {}
    }
    return text;
}

} // namespace core
} // namespace silex
