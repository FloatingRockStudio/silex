/// @file GenericResolver.cpp
/// @brief Public API implementation of the main path/context resolution engine.

#include "private/GenericResolver_p.h"

#include "expression/ExpressionEvaluator.h"
#include "expression/ExpressionParser.h"
#include "registry/Registry.h"
#include "schema/FileSchemaLoader.h"
#include "schema/SchemaUtils.h"
#include "ResolverHelpers.h"
#include "util/Logging.h"
#include "util/Utils.h"

#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

namespace silex {
namespace resolvers {

namespace {

bool isAncestorPath(
    const std::optional<std::string>& ancestor,
    const std::optional<std::string>& descendant) {
    if (!ancestor.has_value() || !descendant.has_value()) {
        return false;
    }

    std::filesystem::path ancestorPath = std::filesystem::path(*ancestor).lexically_normal();
    std::filesystem::path descendantPath = std::filesystem::path(*descendant).lexically_normal();

    if (ancestorPath == descendantPath) {
        return false;
    }

    auto ancestorIt = ancestorPath.begin();
    auto descendantIt = descendantPath.begin();
    for (; ancestorIt != ancestorPath.end() && descendantIt != descendantPath.end(); ++ancestorIt, ++descendantIt) {
        if (*ancestorIt != *descendantIt) {
            return false;
        }
    }

    return ancestorIt == ancestorPath.end() && descendantIt != descendantPath.end();
}

void filterPathMatches(
    SilexPathResolve& result,
    const std::optional<std::vector<std::string>>& endpoint,
    bool includeChildren) {
    if (!endpoint.has_value() || endpoint->empty()) {
        return;
    }

    std::vector<SilexResolveMatch> filteredMatches;
    const auto sourceMatches = result.matches.empty()
        ? std::vector<SilexResolveMatch>{SilexResolveMatch{
            result.status,
            std::nullopt,
            result.resolvedPath,
            std::nullopt,
            result.context,
            result.schemaUid,
            result.schemaEndpoint,
            result.schemaEndpointPath,
            result.usedDeprecatedTraversal,
        }}
        : result.matches;

    for (const auto& match : sourceMatches) {
        if (!match.schemaEndpoint.has_value()) {
            continue;
        }
        if (std::find(endpoint->begin(), endpoint->end(), *match.schemaEndpoint) != endpoint->end()) {
            filteredMatches.push_back(match);
        }
    }

    if (filteredMatches.empty()) {
        return;
    }

    std::vector<SilexResolveMatch> collapsedMatches;
    for (size_t i = 0; i < filteredMatches.size(); ++i) {
        const auto& candidate = filteredMatches[i];
        bool dropCandidate = false;

        for (size_t j = 0; j < filteredMatches.size(); ++j) {
            if (i == j) {
                continue;
            }

            const auto& other = filteredMatches[j];
            if (candidate.schemaEndpoint != other.schemaEndpoint) {
                continue;
            }

            if (!includeChildren && isAncestorPath(other.resolvedPath, candidate.resolvedPath)) {
                dropCandidate = true;
                break;
            }

            if (includeChildren && isAncestorPath(candidate.resolvedPath, other.resolvedPath)) {
                dropCandidate = true;
                break;
            }
        }

        if (!dropCandidate) {
            collapsedMatches.push_back(candidate);
        }
    }

    if (collapsedMatches.empty()) {
        collapsedMatches = filteredMatches;
    }

    result.matches = collapsedMatches;
    result.resolvedPath = collapsedMatches.front().resolvedPath;
    result.context = collapsedMatches.front().context;
    result.schemaUid = collapsedMatches.front().schemaUid;
    result.schemaEndpoint = collapsedMatches.front().schemaEndpoint;
    result.schemaEndpointPath = collapsedMatches.front().schemaEndpointPath;
    result.usedDeprecatedTraversal = collapsedMatches.front().usedDeprecatedTraversal;
    result.status = ResolverStatus::Success;
    if (collapsedMatches.size() > 1) {
        result.status = result.status | ResolverStatus::Ambiguous;
    }
}

Json::Value anyToJsonValue(const std::any& value) {
    if (auto* jsonValue = std::any_cast<Json::Value>(&value)) {
        return *jsonValue;
    }
    if (auto* stringValue = std::any_cast<std::string>(&value)) {
        return Json::Value(*stringValue);
    }
    if (auto* intValue = std::any_cast<int>(&value)) {
        return Json::Value(*intValue);
    }
    if (auto* boolValue = std::any_cast<bool>(&value)) {
        return Json::Value(*boolValue);
    }
    if (auto* doubleValue = std::any_cast<double>(&value)) {
        return Json::Value(*doubleValue);
    }
    if (auto* stringArray = std::any_cast<std::vector<std::string>>(&value)) {
        Json::Value array(Json::arrayValue);
        for (const auto& item : *stringArray) {
            array.append(item);
        }
        return array;
    }
    if (auto* mapValue = std::any_cast<std::map<std::string, std::any>>(&value)) {
        Json::Value object(Json::objectValue);
        for (const auto& [key, nestedValue] : *mapValue) {
            object[key] = anyToJsonValue(nestedValue);
        }
        return object;
    }

    return Json::Value(Json::nullValue);
}

}

// MARK: Public API

GenericResolver::GenericResolver(
    const SilexParseOptions& options,
    const std::string& schema,
    const std::string& configId)
    : m_impl(std::make_unique<Impl>(
          core::Registry::instance(configId), options, configId, schema)) {}

GenericResolver::~GenericResolver() = default;
GenericResolver::GenericResolver(GenericResolver&&) noexcept = default;
GenericResolver& GenericResolver::operator=(GenericResolver&&) noexcept = default;

SilexContextResolve GenericResolver::contextFromPath(
    const std::string& path,
    const std::optional<std::vector<std::string>>& endpoint,
    const std::optional<std::vector<std::string>>& schema) {

    auto logger = core::getLogger(core::LoggerNames::GenericResolver);
    logger->info("contextFromPath: {}", path);

    m_impl->ensureInitialized();

    auto matchingSchemas = m_impl->getMatchingSchemas(
        schema.has_value() ? schema : m_impl->defaultSchema, path);

    if (matchingSchemas.empty()) {
        return SilexContextResolve{ResolverStatus::Error, path};
    }

    SilexContextResolve bestResult;
    bestResult.status = ResolverStatus::Error;

    for (const auto& schemaInfo : matchingSchemas) {
        auto fullSchema = m_impl->loader.loadSchema(schemaInfo.uid);
        auto segmenter = m_impl->registry.getSegmenter(schemaInfo.segmenterUid);

        if (!segmenter) {
            logger->warn("Segmenter not found: {}", schemaInfo.segmenterUid);
            continue;
        }

        if (!segmenter->matchesRoot(schemaInfo.rootPath, path)) {
            continue;
        }

        auto segments = segmenter->splitPath(schemaInfo.rootPath, path);

        if (m_impl->options.segmentLimit) {
            auto [trimmed, _] = core::trimList(segments, m_impl->options.segmentLimit);
            segments = trimmed;
        }

        ContextMap workingContext;
        auto result = m_impl->resolvePathRecursive(
            segments, 0, fullSchema->rootSegments,
            schemaInfo.rootPath, workingContext, fullSchema);

        result.sourcePath = path;
        result.schemaUid = schemaInfo.uid;

        if (hasFlag(result.status, ResolverStatus::Success)) {
            if (endpoint) {
                bool epMatch = false;
                if (result.schemaEndpoint) {
                    for (const auto& ep : *endpoint) {
                        if (ep == *result.schemaEndpoint) {
                            epMatch = true;
                            break;
                        }
                    }
                }
                if (!epMatch) {
                    continue;
                }
            }

            if (m_impl->options.includeDeprecated && !m_impl->preferDeprecated) {
                ContextMap deprecatedContext;
                bool previousPreferDeprecated = m_impl->preferDeprecated;
                m_impl->preferDeprecated = true;
                auto deprecatedResult = m_impl->resolvePathRecursive(
                    segments, 0, fullSchema->rootSegments,
                    schemaInfo.rootPath, deprecatedContext, fullSchema);
                m_impl->preferDeprecated = previousPreferDeprecated;

                deprecatedResult.sourcePath = path;
                deprecatedResult.schemaUid = schemaInfo.uid;

                if (hasFlag(deprecatedResult.status, ResolverStatus::Success) &&
                    deprecatedResult.resolvedPath && result.resolvedPath &&
                    *deprecatedResult.resolvedPath != *result.resolvedPath) {

                    SilexContextResolve combined = result;
                    combined.status = ResolverStatus::Success | ResolverStatus::Ambiguous;
                    combined.resolvedPath = result.resolvedPath;
                    combined.context = workingContext;
                    combined.schemaUid = schemaInfo.uid;
                    combined.usedDeprecatedTraversal = true;

                    SilexResolveMatch canonicalMatch;
                    canonicalMatch.status = ResolverStatus::Success;
                    canonicalMatch.resolvedPath = result.resolvedPath;
                    canonicalMatch.context = workingContext;
                    canonicalMatch.schemaUid = schemaInfo.uid;
                    canonicalMatch.schemaEndpoint = result.schemaEndpoint;
                    canonicalMatch.schemaEndpointPath = result.schemaEndpointPath;
                    canonicalMatch.usedDeprecatedTraversal = false;

                    SilexResolveMatch deprecatedMatch;
                    deprecatedMatch.status = ResolverStatus::Success;
                    deprecatedMatch.resolvedPath = deprecatedResult.resolvedPath;
                    deprecatedMatch.context = deprecatedContext;
                    deprecatedMatch.schemaUid = schemaInfo.uid;
                    deprecatedMatch.schemaEndpoint = deprecatedResult.schemaEndpoint;
                    deprecatedMatch.schemaEndpointPath = deprecatedResult.schemaEndpointPath;
                    deprecatedMatch.usedDeprecatedTraversal = true;

                    combined.matches.push_back(canonicalMatch);
                    combined.matches.push_back(deprecatedMatch);
                    return combined;
                }
            }

            return result;
        }

        if (result.resolvedPath && (!bestResult.resolvedPath ||
            result.resolvedPath->size() > bestResult.resolvedPath->size())) {
            bestResult = result;
        }
    }

    if (bestResult.status == ResolverStatus::Error) {
        bestResult.sourcePath = path;
    }
    return bestResult;
}

SilexPathResolve GenericResolver::pathFromContext(
    const ContextMap& context,
    const std::optional<std::vector<std::string>>& endpoint,
    const std::optional<std::vector<std::string>>& schema) {
    return pathFromContext(context, endpoint, schema, false);
}

SilexPathResolve GenericResolver::pathFromContext(
    const ContextMap& context,
    const std::optional<std::vector<std::string>>& endpoint,
    const std::optional<std::vector<std::string>>& schema,
    bool includeChildren) {

    auto logger = core::getLogger(core::LoggerNames::GenericResolver);
    logger->info("pathFromContext");

    m_impl->ensureInitialized();

    ContextMap workingContext = context;

    auto matchingSchemas = m_impl->getMatchingSchemas(
        schema.has_value() ? schema : m_impl->defaultSchema);

    if (matchingSchemas.empty()) {
        SilexPathResolve result;
        result.status = ResolverStatus::Error;
        result.context = workingContext;
        return result;
    }

    SilexPathResolve bestResult;
    bestResult.status = ResolverStatus::Error;

    for (const auto& schemaInfo : matchingSchemas) {
        auto fullSchema = m_impl->loader.loadSchema(schemaInfo.uid);

        m_impl->prepopulateContext(fullSchema->rootSegments, workingContext, fullSchema);

        auto result = m_impl->writePathRecursive(
            fullSchema->rootSegments, schemaInfo.rootPath,
            workingContext, fullSchema, endpoint, 0);

        result.schemaUid = schemaInfo.uid;
        filterPathMatches(result, endpoint, includeChildren);

        if (hasFlag(result.status, ResolverStatus::Success)) {
            if (m_impl->options.includeDeprecated && result.usedDeprecatedTraversal) {
                SilexParseOptions savedOptions = m_impl->options;
                m_impl->options.includeDeprecated = false;
                auto canonicalResult = m_impl->writePathRecursive(
                    fullSchema->rootSegments, schemaInfo.rootPath,
                    workingContext, fullSchema, endpoint, 0);
                m_impl->options = savedOptions;

                if (hasFlag(canonicalResult.status, ResolverStatus::Success)) {
                    SilexPathResolve combined;
                    combined.status = ResolverStatus::Success | ResolverStatus::Ambiguous;
                    combined.resolvedPath = canonicalResult.resolvedPath;
                    combined.context = workingContext;
                    combined.schemaUid = schemaInfo.uid;
                    combined.usedDeprecatedTraversal = true;

                    SilexResolveMatch canonicalMatch;
                    canonicalMatch.status = ResolverStatus::Success;
                    canonicalMatch.resolvedPath = canonicalResult.resolvedPath;
                    canonicalMatch.context = workingContext;
                    canonicalMatch.schemaUid = schemaInfo.uid;
                    canonicalMatch.schemaEndpoint = canonicalResult.schemaEndpoint;
                    canonicalMatch.schemaEndpointPath = canonicalResult.schemaEndpointPath;
                    canonicalMatch.usedDeprecatedTraversal = true;

                    SilexResolveMatch deprecatedMatch;
                    deprecatedMatch.status = ResolverStatus::Success;
                    deprecatedMatch.resolvedPath = result.resolvedPath;
                    deprecatedMatch.context = workingContext;
                    deprecatedMatch.schemaUid = schemaInfo.uid;
                    deprecatedMatch.schemaEndpoint = result.schemaEndpoint;
                    deprecatedMatch.schemaEndpointPath = result.schemaEndpointPath;
                    deprecatedMatch.usedDeprecatedTraversal = true;

                    combined.matches.push_back(canonicalMatch);
                    combined.matches.push_back(deprecatedMatch);
                    filterPathMatches(combined, endpoint, includeChildren);
                    return combined;
                }
            }

            if (m_impl->options.includeDeprecated && !result.usedDeprecatedTraversal) {
                m_impl->preferDeprecated = true;
                auto deprecatedResult = m_impl->writePathRecursive(
                    fullSchema->rootSegments, schemaInfo.rootPath,
                    workingContext, fullSchema, endpoint, 0);
                m_impl->preferDeprecated = false;

                if (hasFlag(deprecatedResult.status, ResolverStatus::Success) &&
                    deprecatedResult.resolvedPath != result.resolvedPath) {
                    SilexPathResolve combined;
                    combined.status = ResolverStatus::Success | ResolverStatus::Ambiguous;
                    combined.resolvedPath = result.resolvedPath;
                    combined.context = workingContext;
                    combined.schemaUid = schemaInfo.uid;
                    combined.usedDeprecatedTraversal = true;

                    SilexResolveMatch canonicalMatch;
                    canonicalMatch.status = ResolverStatus::Success;
                    canonicalMatch.resolvedPath = result.resolvedPath;
                    canonicalMatch.context = workingContext;
                    canonicalMatch.schemaUid = schemaInfo.uid;
                    canonicalMatch.schemaEndpoint = result.schemaEndpoint;
                    canonicalMatch.schemaEndpointPath = result.schemaEndpointPath;
                    canonicalMatch.usedDeprecatedTraversal = false;

                    SilexResolveMatch deprecatedMatch;
                    deprecatedMatch.status = ResolverStatus::Success;
                    deprecatedMatch.resolvedPath = deprecatedResult.resolvedPath;
                    deprecatedMatch.context = workingContext;
                    deprecatedMatch.schemaUid = schemaInfo.uid;
                    deprecatedMatch.schemaEndpoint = deprecatedResult.schemaEndpoint;
                    deprecatedMatch.schemaEndpointPath = deprecatedResult.schemaEndpointPath;
                    deprecatedMatch.usedDeprecatedTraversal = true;

                    combined.matches.push_back(canonicalMatch);
                    combined.matches.push_back(deprecatedMatch);
                    filterPathMatches(combined, endpoint, includeChildren);
                    return combined;
                }
            }
            return result;
        }

        if (result.resolvedPath && (!bestResult.resolvedPath ||
            result.resolvedPath->size() > bestResult.resolvedPath->size())) {
            bestResult = result;
        }
    }

    return bestResult;
}

std::vector<std::string> GenericResolver::availableTemplates(
    const std::optional<std::vector<std::string>>& schema) {

    m_impl->ensureInitialized();

    std::vector<std::string> templates;
    auto schemas = m_impl->getMatchingSchemas(schema);

    for (const auto& schemaInfo : schemas) {
        auto fullSchema = m_impl->loader.loadSchema(schemaInfo.uid);
        for (const auto& [name, _] : fullSchema->templates) {
            if (std::find(templates.begin(), templates.end(), name) == templates.end()) {
                templates.push_back(name);
            }
        }
    }

    return templates;
}

ContextMap GenericResolver::parseTemplateValue(
    const std::string& value,
    const std::string& templateName,
    const std::optional<ContextMap>& context,
    const std::optional<std::vector<std::string>>& schema) {
    m_impl->ensureInitialized();

    auto schemas = m_impl->getMatchingSchemas(schema);

    for (const auto& schemaInfo : schemas) {
        auto fullSchema = m_impl->loader.loadSchema(schemaInfo.uid);

        auto tmplIt = fullSchema->templates.find(templateName);
        if (tmplIt == fullSchema->templates.end()) continue;

        Json::Value templateRef(Json::objectValue);
        templateRef["template"] = templateName;

        std::map<std::string, std::map<std::string, Json::Value>> templateJsonMap;
        for (const auto& [tmplName, tmplDef] : fullSchema->templates) {
            std::map<std::string, Json::Value> normalizedTemplate;
            for (const auto& [key, anyValue] : tmplDef) {
                normalizedTemplate[key] = anyToJsonValue(anyValue);
            }
            templateJsonMap[tmplName] = std::move(normalizedTemplate);
        }

        auto templateSegment = core::parseSegmentFromJson(templateName, templateRef, templateJsonMap);
        if (!templateSegment || templateSegment->parse.empty()) continue;

        std::optional<std::smatch> templateMatch;
        if (templateSegment->pattern && !templateSegment->pattern->empty()) {
            try {
                std::regex pattern(*templateSegment->pattern);
                std::smatch match;
                if (!std::regex_match(value, match, pattern)) continue;
                templateMatch = match;
            } catch (const std::regex_error&) {
                continue;
            }
        }

        auto parseResult = m_impl->evaluateParseExpressions(
            *templateSegment,
            value,
            "",
            context.value_or(ContextMap{}),
            fullSchema,
            templateMatch ? &*templateMatch : nullptr);

        if (!parseResult.success) continue;

        ContextMap resultCtx;
        for (const auto& [key, resolved] : parseResult.outputs) {
            std::string leafKey = key;
            auto dotPos = leafKey.rfind('.');
            if (dotPos != std::string::npos) {
                leafKey = leafKey.substr(dotPos + 1);
            }
            resultCtx[leafKey] = resolved.value;
        }
        resultCtx["value"] = std::any(value);
        return resultCtx;
    }

    return {};
}

std::string GenericResolver::formatTemplateValue(
    const ContextMap& context,
    const std::string& templateName,
    const std::optional<std::vector<std::string>>& schema) {

    auto logger = core::getLogger(core::LoggerNames::GenericResolver);
    m_impl->ensureInitialized();

    auto schemas = m_impl->getMatchingSchemas(schema);

    // Flatten context for variable resolution
    std::map<std::string, std::string> flatContext;
    m_impl->flattenContext(context, "", flatContext);

    // For template mode, also add flat keys with common prefixes
    // so {context.asset} resolves from flat key "asset"
    std::map<std::string, std::string> augmentedContext = flatContext;
    for (const auto& [k, v] : flatContext) {
        if (k.find('.') == std::string::npos) {
            // Add with "context." prefix for the common case
            if (augmentedContext.find("context." + k) == augmentedContext.end()) {
                augmentedContext["context." + k] = v;
            }
            // Also with "entity." prefix
            if (augmentedContext.find("entity." + k) == augmentedContext.end()) {
                augmentedContext["entity." + k] = v;
            }
        }
    }
    flatContext = augmentedContext;

    for (const auto& schemaInfo : schemas) {
        auto fullSchema = m_impl->loader.loadSchema(schemaInfo.uid);

        auto tmplIt = fullSchema->templates.find(templateName);
        if (tmplIt == fullSchema->templates.end()) continue;

        auto writeIt = tmplIt->second.find("format");
        if (writeIt == tmplIt->second.end()) continue;

        // Collect write expressions: handle both string and array-with-when formats
        std::vector<std::string> formatExprs;

        if (auto* s = std::any_cast<std::string>(&writeIt->second)) {
            formatExprs.push_back(*s);
        } else if (auto* jval = std::any_cast<Json::Value>(&writeIt->second)) {
            if (jval->isArray()) {
                for (Json::ArrayIndex i = 0; i < jval->size(); ++i) {
                    const auto& entry = (*jval)[i];
                    if (!entry.isObject()) continue;
                    // Check 'when' condition
                    if (entry.isMember("when")) {
                        const auto& when = entry["when"];
                        bool conditionMet = false;
                        if (when.isMember("key") && when.isMember("exists")) {
                            std::string key = when["key"].asString();
                            bool shouldExist = when["exists"].asBool();
                            bool exists = flatContext.count(key) > 0;
                            conditionMet = (exists == shouldExist);
                        } else if (when.isMember("keys") && when.isMember("exists")) {
                            bool shouldExist = when["exists"].asBool();
                            bool allMatch = true;
                            for (Json::ArrayIndex k = 0; k < when["keys"].size(); ++k) {
                                std::string key = when["keys"][k].asString();
                                bool exists = flatContext.count(key) > 0;
                                if (exists != shouldExist) { allMatch = false; break; }
                            }
                            conditionMet = allMatch;
                        }
                        if (!conditionMet) continue;
                    }
                    if (entry.isMember("expression")) {
                        formatExprs.push_back(entry["expression"].asString());
                    }
                }
            } else if (jval->isString()) {
                formatExprs.push_back(jval->asString());
            }
        }

        if (formatExprs.empty()) continue;

        // Try each matching write expression
        for (const auto& writeExpr : formatExprs) {
            // Simple template substitution: replace {key} with context values
            std::string result = writeExpr;
            static const std::regex varPat(R"(\{([^}:]+)(?::([^}]+))?\})");
            std::string resolved;
            std::string remaining = result;
            std::smatch m;
            bool allResolved = true;

            while (std::regex_search(remaining, m, varPat)) {
                resolved += m.prefix().str();
                std::string key = m[1].str();
                auto it = flatContext.find(key);
                if (it != flatContext.end()) {
                    resolved += it->second;
                } else {
                    allResolved = false;
                    break;
                }
                remaining = m.suffix().str();
            }

            if (allResolved) {
                resolved += remaining;
                // Check if it contains a functor call
                if (resolved.find('$') != std::string::npos) {
                    // Parse and evaluate the expression (with cache)
                    std::string vftCacheKey = writeExpr;
                    for (const auto& [ak, av] : schemaInfo.functorAliases) {
                        vftCacheKey += '\0'; vftCacheKey += ak; vftCacheKey += '='; vftCacheKey += av;
                    }
                    SilexExpressionGraph* vftGraphPtr;
                    auto vftCacheIt = m_impl->expressionCache.find(vftCacheKey);
                    if (vftCacheIt != m_impl->expressionCache.end()) {
                        vftGraphPtr = &vftCacheIt->second;
                    } else {
                        core::ExpressionParser parser(m_impl->registry, schemaInfo.functorAliases);
                        auto [inserted, _] = m_impl->expressionCache.emplace(vftCacheKey, parser.parseExpressions({writeExpr}));
                        vftGraphPtr = &inserted->second;
                    }
                    auto& graph = *vftGraphPtr;
                    FunctorContext functorCtx;
                    functorCtx.context = context;
                    // Inject flat context variables for resolution
                    for (const auto& [k, v] : flatContext) {
                        if (functorCtx.context.find(k) == functorCtx.context.end()) {
                            functorCtx.context[k] = std::any(v);
                        }
                    }
                    core::ExpressionEvaluator evaluator(m_impl->registry);

                    if (!graph.expressions.empty()) {
                        // Use full graph evaluation to handle nested functors
                        auto graphResult = evaluator.evaluateGraph(graph, functorCtx, fullSchema->config);
                        if (graphResult.success) {
                            auto outIt = graphResult.outputs.find("output");
                            if (outIt != graphResult.outputs.end()) {
                                if (auto* s = std::any_cast<std::string>(&outIt->second.value)) {
                                    return *s;
                                }
                            }
                        }
                    }
                } else {
                    return resolved;
                }
            }
        }
    }

    return "";
}

std::optional<std::string> GenericResolver::schemaFromPath(const std::string& path) {
    m_impl->ensureInitialized();

    auto schemas = m_impl->getMatchingSchemas(std::nullopt, path);
    if (!schemas.empty()) {
        return schemas[0].uid;
    }
    return std::nullopt;
}

} // namespace resolvers
} // namespace silex
