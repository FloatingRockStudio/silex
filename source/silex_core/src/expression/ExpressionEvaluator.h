#pragma once

/// @file ExpressionEvaluator.h
/// @brief Evaluates expression graphs by executing functors in dependency order.

#include <silex/interfaces/IExpressionEvaluator.h>
#include <silex/structs.h>

namespace silex {

class IFunctor;

namespace core {

class Registry;

/// Evaluates individual expressions and expression graphs using registered functors.
class ExpressionEvaluator : public IExpressionEvaluator {
public:
    /// Construct an evaluator using the given component registry.
    explicit ExpressionEvaluator(Registry& registry);
    ~ExpressionEvaluator() override;

    ExpressionEvaluator(const ExpressionEvaluator&) = delete;
    ExpressionEvaluator& operator=(const ExpressionEvaluator&) = delete;
    ExpressionEvaluator(ExpressionEvaluator&&) = delete;
    ExpressionEvaluator& operator=(ExpressionEvaluator&&) = delete;

    ParseResult evaluateGraph(
        const SilexExpressionGraph& graph,
        const FunctorContext& context,
        const SilexConfig& config) override;

    ParseResult evaluateParseExpression(
        const SilexExpression& expression,
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context,
        const SilexConfig& config) override;

    FormatResult evaluateFormatExpression(
        const SilexExpression& expression,
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context,
        const SilexConfig& config) override;
    /// Get the registry reference.
    Registry& registry() { return m_registry; }
private:
    /// Get or create a functor instance from the registry.
    std::shared_ptr<IFunctor> getFunctorInstance(const SilexFunctorInfo& info);

    /// Build the context passed to a functor's read/write call.
    FunctorContext createFunctorContext(
        const SilexFunctorInfo& info,
        const FunctorContext& context,
        const SilexConfig& config);

    /// Flatten a nested map into dot-notation key-value pairs.
    void flattenMap(
        const std::map<std::string, std::any>& m,
        const std::string& prefix,
        std::map<std::string, std::string>& out);

    /// Collect all available variables for graph evaluation.
    std::map<std::string, std::string> prepareGraphVariables(
        const SilexExpressionGraph& graph,
        const FunctorContext& context,
        const SilexConfig& config);

    /// Resolve inputs for one expression using graph outputs and variables.
    std::vector<FunctorInput> resolveExpressionInputs(
        const SilexExpression& expression,
        int exprIdx,
        const SilexExpressionGraph& graph,
        const std::map<int, std::map<std::string, ResolvedValue>>& expressionOutputs,
        const std::map<std::string, std::string>& availableVars);

    /// Resolve a single dynamic input from graph connections or variables.
    FunctorInput resolveDynamicInput(
        int exprIdx,
        int inputIdx,
        const SilexExpressionGraph& graph,
        const std::map<int, std::map<std::string, ResolvedValue>>& expressionOutputs,
        const std::map<std::string, std::string>& availableVars,
        const ExpressionInput& exprInput);

    /// Substitute variable references in an input value.
    FunctorInput formatInputValue(
        const FunctorInput& value,
        const std::map<std::string, std::string>& availableVars);

    /// Evaluate a template-mode expression (no arrow, string interpolation).
    FormatResult evaluateTemplateExpression(
        const SilexExpression& expression,
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context,
        const SilexConfig& config);

    Registry& m_registry;
};

} // namespace core
} // namespace silex
