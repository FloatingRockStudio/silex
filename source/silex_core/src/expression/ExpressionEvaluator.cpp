// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file ExpressionEvaluator.cpp
/// @brief Implementation of expression graph evaluation with functor execution.

#include "ExpressionEvaluator.h"
#include "DAG.h"
#include "util/Logging.h"
#include "registry/Registry.h"
#include "util/Utils.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <variant>

namespace silex {
namespace core {

// MARK: Helpers

static std::string functorInputToString(const FunctorInput& input) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else return "";
    }, input);
}

static std::string formatString(const std::string& tmpl,
                                const std::map<std::string, std::string>& vars) {
    std::string result = tmpl;
    for (const auto& [key, val] : vars) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), val);
            pos += val.size();
        }
    }
    return result;
}

// MARK: Construction

ExpressionEvaluator::ExpressionEvaluator(Registry& registry)
    : m_registry(registry) {}

ExpressionEvaluator::~ExpressionEvaluator() = default;

// MARK: Private

std::shared_ptr<IFunctor> ExpressionEvaluator::getFunctorInstance(const SilexFunctorInfo& info) {
    return m_registry.getFunctor(info.uid);
}

FunctorContext ExpressionEvaluator::createFunctorContext(
    const SilexFunctorInfo& info,
    const FunctorContext& context,
    const SilexConfig& config) {

    ContextMap variables = config.globalVariables;

    auto it = config.functorVariables.find(info.uid);
    if (it != config.functorVariables.end()) {
        if (auto* nested = std::any_cast<ContextMap>(&it->second)) {
            for (const auto& [k, v] : *nested) {
                variables[k] = v;
            }
        }
    }

    for (const auto& [k, v] : context.variables) {
        variables[k] = v;
    }

    return FunctorContext{context.context, context.parent, context.segment, variables};
}

void ExpressionEvaluator::flattenMap(
    const std::map<std::string, std::any>& m,
    const std::string& prefix,
    std::map<std::string, std::string>& out) {

    for (const auto& [k, v] : m) {
        std::string fullKey = prefix.empty() ? k : prefix + "." + k;
        if (auto* s = std::any_cast<std::string>(&v)) {
            out[fullKey] = *s;
        } else if (auto* nested = std::any_cast<std::map<std::string, std::any>>(&v)) {
            flattenMap(*nested, fullKey, out);
        } else if (auto* i = std::any_cast<int>(&v)) {
            out[fullKey] = std::to_string(*i);
        }
    }
}

std::map<std::string, std::string> ExpressionEvaluator::prepareGraphVariables(
    const SilexExpressionGraph& graph,
    const FunctorContext& context,
    const SilexConfig& config) {

    std::map<std::string, std::string> variables;

    flattenMap(config.globalVariables, "", variables);
    flattenMap(context.variables, "", variables);
    flattenMap(context.context, "", variables);

    return variables;
}

std::vector<FunctorInput> ExpressionEvaluator::resolveExpressionInputs(
    const SilexExpression& expression,
    int exprIdx,
    const SilexExpressionGraph& graph,
    const std::map<int, std::map<std::string, ResolvedValue>>& expressionOutputs,
    const std::map<std::string, std::string>& availableVars) {

    std::vector<FunctorInput> resolved;

    for (size_t inputIdx = 0; inputIdx < expression.inputs.size(); ++inputIdx) {
        const auto& exprInput = expression.inputs[inputIdx];

        if (exprInput.isDynamic) {
            auto value = resolveDynamicInput(
                exprIdx, static_cast<int>(inputIdx), graph,
                expressionOutputs, availableVars, exprInput);
            resolved.push_back(value);
        } else {
            resolved.push_back(exprInput.value);
        }
    }

    return resolved;
}

FunctorInput ExpressionEvaluator::resolveDynamicInput(
    int exprIdx,
    int inputIdx,
    const SilexExpressionGraph& graph,
    const std::map<int, std::map<std::string, ResolvedValue>>& expressionOutputs,
    const std::map<std::string, std::string>& availableVars,
    const ExpressionInput& exprInput) {

    for (const auto& [src, tgt] : graph.connections) {
        if (tgt.first == exprIdx && tgt.second == inputIdx) {
            auto outIt = expressionOutputs.find(src.first);
            if (outIt != expressionOutputs.end()) {
                const auto& srcOutputs = outIt->second;
                const auto& srcExpr = graph.expressions[src.first];
                if (src.second < static_cast<int>(srcExpr.outputs.size())) {
                    const std::string& outputName = srcExpr.outputs[src.second];
                    auto rvIt = srcOutputs.find(outputName);
                    if (rvIt != srcOutputs.end()) {
                        if (auto* s = std::any_cast<std::string>(&rvIt->second.value)) {
                            return *s;
                        }
                    }
                } else if (srcExpr.mode == ExpressionMode::Format) {
                    // Write-mode nested expressions store result under "output"
                    auto rvIt = srcOutputs.find("output");
                    if (rvIt != srcOutputs.end()) {
                        if (auto* s = std::any_cast<std::string>(&rvIt->second.value)) {
                            return *s;
                        }
                    }
                }
            }
        }
    }

    return formatInputValue(exprInput.value, availableVars);
}

FunctorInput ExpressionEvaluator::formatInputValue(
    const FunctorInput& value,
    const std::map<std::string, std::string>& availableVars) {

    if (auto* s = std::get_if<std::string>(&value)) {
        return formatString(*s, availableVars);
    }
    return value;
}

FormatResult ExpressionEvaluator::evaluateTemplateExpression(
    const SilexExpression& expression,
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context,
    const SilexConfig& config) {

    auto logger = getLogger(LoggerNames::ExpressionEvaluator);

    const auto& raw = expression.raw;
    auto pipePos = raw.find('|');
    if (pipePos == std::string::npos ||
        raw.substr(0, 9) != "TEMPLATE:" ||
        raw.substr(pipePos + 1, 8) != "FUNCTOR:") {
        return FormatResult{false, "Invalid template expression format", ""};
    }

    std::string templateFormat = raw.substr(9, pipePos - 9);

    if (!expression.functorInfo) {
        return FormatResult{false, "No functor information for template expression", ""};
    }

    auto functor = getFunctorInstance(*expression.functorInfo);
    if (!functor) {
        return FormatResult{false, "Could not instantiate functor", ""};
    }

    auto functorCtx = createFunctorContext(*expression.functorInfo, context, config);

    try {
        auto result = functor->format(inputs, functorCtx);
        if (!result.success) return result;

        std::map<std::string, std::string> templateVars;
        templateVars["_output"] = result.output;

        for (const auto& [k, v] : context.context) {
            if (auto* s = std::any_cast<std::string>(&v)) {
                templateVars[k] = *s;
            }
        }

        std::string finalOutput = formatString(templateFormat, templateVars);
        return FormatResult{true, "Template formatted successfully", finalOutput};
    } catch (const std::exception& e) {
        return FormatResult{false, std::string("Template error: ") + e.what(), ""};
    }
}

// MARK: Public

ParseResult ExpressionEvaluator::evaluateGraph(
    const SilexExpressionGraph& graph,
    const FunctorContext& context,
    const SilexConfig& config) {

    auto logger = getLogger(LoggerNames::ExpressionEvaluator);
    logger->debug("Starting graph evaluation with {} expressions", graph.expressions.size());

    if (graph.expressions.empty()) {
        return ParseResult{true, "Empty graph", {}};
    }

    DAG dag;
    for (size_t i = 0; i < graph.expressions.size(); ++i) {
        dag.addNode(static_cast<int>(i));
    }
    for (const auto& [src, tgt] : graph.connections) {
        if (src.first >= 0 && tgt.first >= 0) {
            dag.addEdge(src.first, tgt.first);
        }
    }

    if (!dag.isDAG()) {
        return ParseResult{false, "Circular dependencies detected in expression graph", {}};
    }

    auto executionOrder = dag.topologicalSort();
    if (executionOrder.size() != graph.expressions.size()) {
        return ParseResult{false, "Error determining execution order", {}};
    }

    auto availableVars = prepareGraphVariables(graph, context, config);
    std::map<int, std::map<std::string, ResolvedValue>> expressionOutputs;

    for (int exprIdx : executionOrder) {
        const auto& expression = graph.expressions[exprIdx];

        auto resolvedInputs = resolveExpressionInputs(
            expression, exprIdx, graph, expressionOutputs, availableVars);

        if (expression.mode == ExpressionMode::Parse) {
            auto result = evaluateParseExpression(expression, resolvedInputs, context, config);
            if (result.success) {
                expressionOutputs[exprIdx] = result.outputs;
            } else {
                return ParseResult{false,
                    "Failed to evaluate read expression " + std::to_string(exprIdx) + ": " + result.message, {}};
            }
        } else {
            auto result = evaluateFormatExpression(expression, resolvedInputs, context, config);
            if (result.success) {
                expressionOutputs[exprIdx] = {{"output", ResolvedValue{result.output, false}}};
            } else {
                return ParseResult{false,
                    "Failed to evaluate write expression " + std::to_string(exprIdx) + ": " + result.message, {}};
            }
        }
    }

    std::map<std::string, ResolvedValue> finalOutputs;
    for (const auto& [exprIdx, outputs] : expressionOutputs) {
        for (const auto& [name, value] : outputs) {
            bool isGraphOutput = std::find(graph.outputs.begin(), graph.outputs.end(), name)
                                 != graph.outputs.end();
            if (isGraphOutput) {
                finalOutputs[name] = value;
            }
        }
    }

    return ParseResult{true, "Graph evaluation completed", finalOutputs};
}

ParseResult ExpressionEvaluator::evaluateParseExpression(
    const SilexExpression& expression,
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context,
    const SilexConfig& config) {

    auto logger = getLogger(LoggerNames::ExpressionEvaluator);

    // Handle identity/passthrough expressions (e.g., "assets->tree")
    if (expression.functorName == "__identity__") {
        ParseResult result;
        result.success = true;
        for (size_t i = 0; i < expression.functorOutputs.size(); ++i) {
            ResolvedValue rv;
            if (i < inputs.size()) {
                if (auto* s = std::get_if<std::string>(&inputs[i])) {
                    rv.value = std::any(*s);
                }
            }
            result.outputs[expression.functorOutputs[i].name] = rv;
        }
        return result;
    }

    if (!expression.functorInfo) {
        return ParseResult{false, "No functor information for expression: " + expression.raw, {}};
    }

    auto functor = getFunctorInstance(*expression.functorInfo);
    if (!functor) {
        return ParseResult{false, "Could not instantiate functor: " + expression.functorInfo->name, {}};
    }

    auto functorCtx = createFunctorContext(*expression.functorInfo, context, config);

    try {
        return functor->parse(inputs, expression.functorOutputs, functorCtx);
    } catch (const std::exception& e) {
        return ParseResult{false,
            "Error executing read functor " + expression.functorInfo->name + ": " + e.what(), {}};
    }
}

FormatResult ExpressionEvaluator::evaluateFormatExpression(
    const SilexExpression& expression,
    const std::vector<FunctorInput>& inputs,
    const FunctorContext& context,
    const SilexConfig& config) {

    auto logger = getLogger(LoggerNames::ExpressionEvaluator);

    if (expression.raw.substr(0, 9) == "TEMPLATE:") {
        return evaluateTemplateExpression(expression, inputs, context, config);
    }

    if (!expression.functorInfo) {
        return FormatResult{false, "No functor information for expression: " + expression.raw, ""};
    }

    auto functor = getFunctorInstance(*expression.functorInfo);
    if (!functor) {
        return FormatResult{false, "Could not instantiate functor: " + expression.functorInfo->name, ""};
    }

    auto functorCtx = createFunctorContext(*expression.functorInfo, context, config);

    try {
        return functor->format(inputs, functorCtx);
    } catch (const std::exception& e) {
        return FormatResult{false,
            "Error executing write functor " + expression.functorInfo->name + ": " + e.what(), ""};
    }
}

} // namespace core
} // namespace silex
