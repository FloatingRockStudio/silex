// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

#pragma once

/// @file Registry.h
/// @brief Unified registry for functors, segmenters, and schema components.

#include <silex/structs.h>
#include <silex/interfaces/IFunctor.h>
#include <silex/interfaces/ISegmenter.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace silex {
namespace core {

/// Factory function type for creating functor instances.
using FunctorFactory = std::function<std::shared_ptr<IFunctor>()>;
/// Factory function type for creating segmenter instances.
using SegmenterFactory = std::function<std::shared_ptr<ISegmenter>()>;

/// Unified registry for all Silex components.
class Registry {
public:
    /// Get or create a singleton instance of the registry.
    static Registry& instance(const std::string& key = "silex");

    /// Construct a registry with the given name.
    Registry(const std::string& name = "default");
    ~Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = delete;
    Registry& operator=(Registry&&) = delete;

    /// Get the registry name.
    const std::string& name() const;

    // MARK: Functors

    /// Register a functor with its metadata and factory.
    void registerFunctor(const SilexFunctorInfo& info, FunctorFactory factory);

    /// Get functor info by name, alias, module.name, package.name, or UID.
    std::optional<SilexFunctorInfo> getFunctorInfo(const std::string& name) const;

    /// Get functor instance by UID, creating and caching if needed.
    std::shared_ptr<IFunctor> getFunctor(const std::string& uid);

    /// Get all registered functor UIDs.
    std::vector<std::string> getAllFunctorUids() const;

    // MARK: Segmenters

    /// Register a segmenter with its metadata and factory.
    void registerSegmenter(const ExternalResource& info, SegmenterFactory factory);

    /// Get segmenter info by UID, name, or package.name.
    std::optional<ExternalResource> getSegmenterInfo(const std::string& name) const;

    /// Get segmenter instance by UID, creating and caching if needed.
    std::shared_ptr<ISegmenter> getSegmenter(const std::string& uid);

    // MARK: Schemas

    /// Register a schema with its metadata.
    void registerSchema(const SilexSchemaInfo& info);

    /// Cache a loaded schema instance.
    void cacheSchema(const std::string& uid, std::shared_ptr<SilexSchema> schema);

    /// Get schema info by UID.
    std::optional<SilexSchemaInfo> getSchemaInfo(const std::string& uid) const;

    /// Get cached schema by UID.
    std::shared_ptr<SilexSchema> getSchema(const std::string& uid) const;

    // MARK: Lifecycle

    /// Clear all registered components and caches.
    void clear();

    /// Clear only caches (keep registrations).
    void clearCache();

private:
    std::string m_name;

    // Functor storage
    std::map<std::string, SilexFunctorInfo> m_functorInfo;
    std::map<std::string, std::shared_ptr<IFunctor>> m_functorCache;
    std::map<std::string, FunctorFactory> m_functorFactories;

    // Segmenter storage
    std::map<std::string, ExternalResource> m_segmenterInfo;
    std::map<std::string, std::shared_ptr<ISegmenter>> m_segmenterCache;
    std::map<std::string, SegmenterFactory> m_segmenterFactories;

    // Schema storage
    std::map<std::string, SilexSchemaInfo> m_schemaInfo;
    std::map<std::string, std::shared_ptr<SilexSchema>> m_schemaCache;
};

} // namespace core
} // namespace silex
