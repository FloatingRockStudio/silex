#pragma once

#include <silex/Box.h>
#include <silex/export.h>
#include <silex/interfaces/IResolver.h>
#include <silex/structs.h>

#include <memory>
#include <string>

namespace silex {
namespace resolvers {

/// Main resolver implementation with comprehensive path/context resolution.
/// Uses PIMPL to hide all implementation details.
class SILEX_API GenericResolver : public IResolver {
public:
    /// Construct a resolver with the given options, schema filter, and config ID.
    GenericResolver(const SilexParseOptions& options = {},
                    const std::string& schema = "",
                    const std::string& configId = "default");
    ~GenericResolver() override;

    GenericResolver(const GenericResolver&) = delete;
    GenericResolver& operator=(const GenericResolver&) = delete;
    GenericResolver(GenericResolver&&) noexcept;
    GenericResolver& operator=(GenericResolver&&) noexcept;

    /// Resolve a path to context.
    SilexContextResolve contextFromPath(
        const std::string& path,
        const std::optional<std::vector<std::string>>& endpoint = std::nullopt,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) override;

    /// Resolve context to path.
    SilexPathResolve pathFromContext(
        const ContextMap& context,
        const std::optional<std::vector<std::string>>& endpoint = std::nullopt,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) override;

    /// Resolve context to path and optionally prefer descendant endpoint matches.
    SilexPathResolve pathFromContext(
        const ContextMap& context,
        const std::optional<std::vector<std::string>>& endpoint,
        const std::optional<std::vector<std::string>>& schema,
        bool includeChildren);

    /// List available template names.
    std::vector<std::string> availableTemplates(
        const std::optional<std::vector<std::string>>& schema = std::nullopt) override;

    /// Extract context from value using template.
    ContextMap parseTemplateValue(
        const std::string& value,
        const std::string& templateName,
        const std::optional<ContextMap>& context = std::nullopt,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) override;

    /// Generate value from context using template.
    std::string formatTemplateValue(
        const ContextMap& context,
        const std::string& templateName,
        const std::optional<std::vector<std::string>>& schema = std::nullopt) override;

    /// Get schema UID matching path.
    std::optional<std::string> schemaFromPath(const std::string& path) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace resolvers
} // namespace silex
