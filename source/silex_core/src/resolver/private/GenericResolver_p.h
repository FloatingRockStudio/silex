#pragma once

/// @file GenericResolver_p.h
/// @brief Private implementation details for GenericResolver (PIMPL).

#include <silex/GenericResolver.h>
#include "schema/FileSchemaLoader.h"
#include "registry/Registry.h"

#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace silex {
namespace resolvers {

/// Private implementation of GenericResolver.
struct GenericResolver::Impl {
    core::Registry& registry;
    core::FileSchemaLoader loader;
    SilexParseOptions options;
    std::string configId;
    std::optional<std::vector<std::string>> defaultSchema;
    bool initialized = false;

    /// When true, writePathRecursive skips non-deprecated segments at levels
    /// where a deprecated sibling exists, forcing the deprecated path.
    bool preferDeprecated = false;

    /// Cache parsed expression graphs keyed by concatenated expression strings.
    std::unordered_map<std::string, SilexExpressionGraph> expressionCache;

    Impl(core::Registry& reg, const SilexParseOptions& opts, const std::string& cfgId,
         const std::string& schema = "");

    /// Preload schemas if not yet initialized.
    void ensureInitialized();

    /// Recursively flatten a nested ContextMap into dot-separated string keys.
    void flattenContext(
        const ContextMap& ctx,
        const std::string& prefix,
        std::map<std::string, std::string>& out);

    /// Get schemas matching the given filter.
    std::vector<SilexSchemaInfo> getMatchingSchemas(
        const std::optional<std::vector<std::string>>& schemaFilter,
        const std::optional<std::string>& path = std::nullopt);

    /// Resolve a single path segment using read expressions.
    SegmentResolve resolveSegmentParse(
        const SilexSegment& segment,
        const std::string& segmentValue,
        const std::string& parentPath,
        const ContextMap& currentContext,
        const std::shared_ptr<SilexSchema>& schema);

    /// Evaluate read expressions for a segment.
    ParseResult evaluateParseExpressions(
        const SilexSegment& segment,
        const std::string& value,
        const std::string& parentPath,
        const ContextMap& context,
        const std::shared_ptr<SilexSchema>& schema,
        const std::smatch* matchGroups = nullptr);

    /// Prepopulate context using format_update_keys and read expressions.
    void prepopulateContext(
        const std::vector<std::shared_ptr<SilexSegment>>& segments,
        ContextMap& context,
        const std::shared_ptr<SilexSchema>& schema);

    /// Evaluate write expressions for a segment.
    FormatResult evaluateFormatExpressions(
        const SilexSegment& segment,
        const ContextMap& context,
        const std::string& parentPath,
        const std::shared_ptr<SilexSchema>& schema);

    /// Recursively resolve path segments (read direction: path -> context).
    SilexContextResolve resolvePathRecursive(
        const std::vector<std::string>& segments,
        size_t segmentIdx,
        const std::vector<std::shared_ptr<SilexSegment>>& schemaSegments,
        const std::string& parentPath,
        ContextMap& context,
        const std::shared_ptr<SilexSchema>& schema,
        const std::string& lastEndpoint = "",
        const std::string& schemaPath = "",
        bool deprecatedTraversal = false);

    /// Recursively build path from context (write direction: context -> path).
    SilexPathResolve writePathRecursive(
        const std::vector<std::shared_ptr<SilexSegment>>& schemaSegments,
        const std::string& parentPath,
        const ContextMap& context,
        const std::shared_ptr<SilexSchema>& schema,
        const std::optional<std::vector<std::string>>& targetEndpoint,
        int depth,
        const std::string& schemaPath = "",
        bool deprecatedTraversal = false);
};

} // namespace resolvers
} // namespace silex
