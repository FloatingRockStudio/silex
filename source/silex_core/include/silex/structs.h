// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

#include <silex/constants.h>

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace silex {

// MARK: Forward Declarations

struct SilexSegment;

// MARK: Type Aliases

/// Input type for functors: string, int, or bool.
using FunctorInput = std::variant<std::string, int, double, bool>;

/// Dot-notation context map.
using ContextMap = std::map<std::string, std::any>;

// MARK: Placeholder

/// Placeholder value for wildcard pattern generation in glob operations.
class PlaceholderValue {
public:
    /// Constructs a placeholder from the given string value.
    explicit PlaceholderValue(const std::string& value) : m_value(value) {}

    /// Returns the underlying placeholder value.
    const std::string& value() const { return m_value; }
    /// Returns the placeholder as a string.
    std::string str() const { return m_value; }

private:
    std::string m_value;
};

// MARK: Configuration

/// Configuration for resolver behavior (case sensitivity, variables).
struct SilexConfig {
    /// Variables available globally across all schemas.
    ContextMap globalVariables;
    /// Variables passed to functor read/write operations.
    ContextMap functorVariables;
    /// Variables used for placeholder substitution in patterns.
    ContextMap placeholderVariables;
    bool caseSensitive = false;
};

// MARK: Functor System

/// Named output from a functor execution.
struct FunctorOutput {
    std::string name;
    /// Valid options for this output.
    std::vector<std::string> options;
};

/// Context passed to functor read/write operations.
struct FunctorContext {
    ContextMap context;
    /// Parent segment name.
    std::string parent;
    /// Current segment name.
    std::string segment;
    ContextMap variables;
};

/// A resolved value that may be ambiguous (multiple matches).
struct ResolvedValue {
    std::any value;
    /// True if resolution produced multiple possible values.
    bool isAmbiguous = false;
};

/// Result of a functor read operation.
struct ParseResult {
    bool success = false;
    std::string message;
    /// Map of output names to their resolved values.
    std::map<std::string, ResolvedValue> outputs;
};

/// Result of a functor write operation.
struct FormatResult {
    bool success = false;
    std::string message;
    std::string output;
    /// Ordered candidate outputs discovered during write evaluation.
    std::vector<std::string> matches;
};

/// Metadata for a registered external component (functor or segmenter).
struct ExternalResource {
    std::string uid;
    std::string name;
    /// Python module path for the resource.
    std::string module;
    Language language = Language::Other;
    /// Rez package providing this resource.
    std::string package;
};

/// Extended metadata for a registered functor (adds aliases).
struct SilexFunctorInfo : ExternalResource {
    /// Alternative names for referencing this functor.
    std::vector<std::string> aliases;
};

// MARK: Expression System

/// An input to an expression (raw string, resolved value, dynamic flag).
struct ExpressionInput {
    /// Original unparsed input string.
    std::string raw;
    /// Parsed input value.
    FunctorInput value;
    /// True if this input is resolved at runtime rather than parse time.
    bool isDynamic = false;
};

/// A parsed functor expression with inputs, outputs, and mode.
struct SilexExpression {
    /// Original unparsed expression string.
    std::string raw;
    /// Name of the functor (or "__identity__" for literal passthrough).
    std::string functorName;
    std::vector<ExpressionInput> inputs;
    std::vector<std::string> outputs;
    /// Resolved functor metadata, if found.
    std::optional<SilexFunctorInfo> functorInfo;
    /// Output descriptors declared by the functor.
    std::vector<FunctorOutput> functorOutputs;
    ExpressionMode mode = ExpressionMode::Parse;
    std::vector<std::string> warnings;
};

/// DAG of expressions with connection and dependency information.
struct SilexExpressionGraph {
    std::vector<SilexExpression> expressions;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    /// Connections: {(outputExprIdx, outputIdx)} -> {(inputExprIdx, inputIdx)}
    /// Expression index -1 means graph input/output.
    std::map<std::pair<int, int>, std::pair<int, int>> connections;
    ExpressionMode mode = ExpressionMode::Parse;
};

// MARK: Path Resolution

/// Options controlling resolver behavior (deprecated inclusion, case sensitivity).
struct SilexParseOptions {
    /// Restrict resolution to these endpoint names.
    std::optional<std::vector<std::string>> endpoint;
    /// Restrict resolution to these schema UIDs.
    std::optional<std::vector<std::string>> schema;
    /// Limit how many segments to resolve (single int or range).
    std::optional<std::variant<int, std::pair<int, int>>> segmentLimit;
    /// Fixed placeholder substitutions to apply during resolution.
    std::map<std::string, std::string> placeholders;
    /// Allow partial matches that don't reach an endpoint.
    bool allowPartial = true;
    bool includeDeprecated = false;
    /// Maximum backtracking iterations before aborting.
    int maxBacktrackIterations = 10;
    int verbosity = 0;
};

/// A single match result from path/context resolution.
struct SilexResolveMatch {
    ResolverStatus status = ResolverStatus::None;
    std::optional<std::string> sourcePath;
    std::optional<std::string> resolvedPath;
    /// Remaining unresolved portion of the path.
    std::optional<std::string> unresolvedPath;
    ContextMap context;
    std::optional<std::string> schemaUid;
    std::optional<std::string> schemaEndpoint;
    /// Full path to the matched endpoint within the schema.
    std::optional<std::string> schemaEndpointPath;
    /// True if resolution traversed deprecated segments.
    bool usedDeprecatedTraversal = false;
};

/// Full result of resolving a path to context.
struct SilexContextResolve {
    ResolverStatus status = ResolverStatus::None;
    std::optional<std::string> sourcePath;
    std::optional<std::string> resolvedPath;
    std::optional<std::string> unresolvedPath;
    ContextMap context;
    std::optional<std::string> schemaUid;
    std::optional<std::string> schemaEndpoint;
    std::optional<std::string> schemaEndpointPath;
    bool usedDeprecatedTraversal = false;
    std::vector<SilexResolveMatch> matches;
};

/// Full result of resolving context to a path.
struct SilexPathResolve {
    ResolverStatus status = ResolverStatus::None;
    std::optional<std::string> resolvedPath;
    ContextMap context;
    /// Context keys required but not provided.
    std::vector<std::string> missingContext;
    std::optional<std::string> schemaUid;
    std::optional<std::string> schemaEndpoint;
    std::optional<std::string> schemaEndpointPath;
    bool usedDeprecatedTraversal = false;
    /// Name of the deepest segment reached during resolution.
    std::optional<std::string> furthestSegment;
    std::vector<SilexResolveMatch> matches;
};

// MARK: Schema Definition

/// Metadata about a discovered .silex schema file.
struct SilexSchemaInfo {
    /// Filesystem path to the schema file.
    std::optional<std::string> path;
    std::string uid;
    /// Root filesystem path for this schema.
    std::string rootPath;
    /// Glob pattern for matching paths under rootPath.
    std::string pathPattern;
    /// Filters to restrict which contexts this schema applies to.
    std::vector<std::map<std::string, std::any>> contextFilters;
    /// UID of the segmenter used to parse paths.
    std::string segmenterUid;
    /// UIDs of functors registered by this schema.
    std::vector<std::string> functorUids;
    /// Map of alias names to functor UIDs.
    std::map<std::string, std::string> functorAliases;
    /// Map of endpoint names to their segment paths.
    std::map<std::string, std::vector<std::string>> endpoints;
    /// UID of a parent schema this one extends.
    std::optional<std::string> extends;
};

/// Maps a segment capture group to a context key path.
struct Target {
    /// Regex capture group index or name.
    std::optional<std::variant<int, std::string>> group;
    /// Variable name to substitute into the target value.
    std::optional<std::string> variable;
    /// Expected value type (e.g. "string", "int").
    std::string type = "string";
    /// True if the target value is an array.
    bool isArray = false;
};

/// Resolve result at the segment level (pattern match + context).
struct SegmentResolve {
    ResolverStatus status = ResolverStatus::None;
    std::string message;
    std::map<std::string, std::any> context;
    /// Context keys that were successfully resolved.
    std::set<std::string> resolvedContext;
    /// Context keys that matched multiple values.
    std::set<std::string> ambiguousContext;
};

/// A node in the path segment tree with expressions and branches.
struct SilexSegment {
    std::string name;
    /// Regex pattern for matching this segment against a path component.
    std::optional<std::string> pattern;
    SegmentFlags flags = SegmentFlags::None;
    /// Expressions evaluated during read operations.
    std::vector<std::map<std::string, std::any>> parse;
    /// Expressions evaluated during write operations.
    std::vector<std::map<std::string, std::any>> format;
    /// Context keys updated during write operations.
    std::vector<std::string> formatUpdateKeys;
    std::map<std::string, std::any> placeholders;
    /// Maps capture group results to context keys.
    std::map<std::string, Target> targets;
    std::weak_ptr<SilexSegment> parent;
    /// Name of the template this segment was instantiated from.
    std::string templateName;
    /// Named child branches, each containing ordered child segments.
    std::map<std::string, std::vector<std::shared_ptr<SilexSegment>>> branches;
    std::string endpoint;
    /// UID of segmenter used for partition resolution.
    std::optional<std::string> partitionSegmenter;
    /// Dynamically resolved child segments from partitioning.
    std::vector<std::shared_ptr<SilexSegment>> partitions;
    /// True if partitions must be resolved in order.
    bool orderedPartitions = false;
    /// Maps branch names to their endpoint names.
    std::map<std::string, std::string> branchEndpoints;
};

/// Complete loaded schema with segments, config, and templates.
struct SilexSchema {
    SilexSchemaInfo info;
    /// Top-level segments in the schema tree.
    std::vector<std::shared_ptr<SilexSegment>> rootSegments;
    SilexConfig config;
    /// Named segment templates for reuse across the schema.
    std::map<std::string, std::map<std::string, std::any>> templates;
};

} // namespace silex
