#pragma once

/// @file ILoader.h
/// @brief Interface for schema discovery and loading.

#include <silex/structs.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace silex {

/// Interface for loading Silex schemas from external sources.
class ILoader {
public:
    virtual ~ILoader() = default;

    /// Initialize databases or file caches to quickly find schemas by UID.
    virtual void preload() = 0;

    /// Match schemas by optional uid pattern, path, or context.
    virtual std::vector<SilexSchemaInfo> match(
        const std::optional<std::string>& uidPattern = std::nullopt,
        const std::optional<std::string>& path = std::nullopt,
        const std::optional<ContextMap>& context = std::nullopt) const = 0;

    /// Load schema info by UID.
    virtual SilexSchemaInfo loadInfo(const std::string& uid) const = 0;

    /// Load a full Silex schema by its unique identifier.
    virtual std::shared_ptr<SilexSchema> loadSchema(const std::string& uid) = 0;

    /// Return a list of all available schema UIDs.
    virtual std::vector<std::string> availableSchema() const = 0;
};

} // namespace silex
