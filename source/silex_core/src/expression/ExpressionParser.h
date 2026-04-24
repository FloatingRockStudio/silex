#pragma once

/// @file ExpressionParser.h
/// @brief Parses functor expression strings into directed acyclic graphs.

#include <silex/interfaces/IExpressionParser.h>
#include <silex/structs.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace silex {
namespace core {

class Registry;

/// Parses expression strings into structured expressions and graphs.
class ExpressionParser : public IExpressionParser {
public:
    /// Construct with a registry and optional schema-level aliases.
    ExpressionParser(Registry& registry,
                     const std::map<std::string, std::string>& schemaAliases = {});
    ~ExpressionParser() override = default;

    ExpressionParser(const ExpressionParser&) = delete;
    ExpressionParser& operator=(const ExpressionParser&) = delete;
    ExpressionParser(ExpressionParser&&) = delete;
    ExpressionParser& operator=(ExpressionParser&&) = delete;

    /// Parse a list of expression strings into an expression graph.
    SilexExpressionGraph parseExpressions(
        const std::vector<std::string>& expressions) override;

    /// Get the registry reference.
    Registry& registry() { return m_registry; }

private:
    /// Internal graph connection data: edges, inputs, and outputs.
    struct GraphConnections {
        std::map<std::pair<int, int>, std::pair<int, int>> connections;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
    };

    /// Parse a single expression string into a list of expressions.
    std::vector<SilexExpression> parseExpressionString(const std::string& exprStr);
    /// Parse the main functor call from an expression string.
    std::optional<SilexExpression> parseMainExpression(
        const std::string& exprStr,
        ExpressionMode mode,
        std::vector<SilexExpression>& nestedExpressions);
    /// Extract nested functor calls from an arguments string.
    std::pair<std::string, std::vector<std::string>> extractNestedFunctors(
        const std::string& argsStr);
    /// Parse a single nested functor call.
    std::optional<SilexExpression> parseNestedFunctor(
        const std::string& functorCall, ExpressionMode parentMode);
    /// Build graph edges from expression inputs and outputs.
    GraphConnections buildGraphConnections(
        const std::vector<SilexExpression>& expressions, ExpressionMode graphMode);
    /// Look up functor metadata by name or alias.
    std::optional<SilexFunctorInfo> resolveFunctorInfo(const std::string& name);
    /// Parse input arguments from a raw arguments string.
    std::vector<ExpressionInput> parseInputs(const std::string& argsStr);
    /// Parse output names and descriptors from a raw outputs string.
    std::pair<std::vector<std::string>, std::vector<FunctorOutput>> parseOutputs(
        const std::string& outputsStr);
    /// Split a comma-separated argument string respecting nested delimiters.
    std::vector<std::string> splitArguments(const std::string& argsStr);
    /// Remove surrounding quotes from a string.
    std::string stripQuotes(const std::string& text);
    /// Extract {variable} names from a template string.
    std::set<std::string> extractVariableNames(const std::string& text);
    /// Extract variable names including those with nested brace syntax.
    std::set<std::string> extractVariablesWithNestedBraces(const std::string& text);
    /// Check if text is a simple {name} variable reference.
    bool isSimpleVariable(const std::string& content);
    /// Check if text is a literal value (quoted string, number, bool).
    bool isLiteralValue(const std::string& text);
    /// Parse a literal value string into a typed FunctorInput.
    FunctorInput parseLiteralValue(const std::string& text);

    Registry& m_registry;
    std::map<std::string, std::string> m_schemaAliases;
};

} // namespace core
} // namespace silex
