// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file Registry.cpp
/// @brief Implementation of the unified component registry.

#include "Registry.h"
#include "BuiltinRegistrar.h"
#include "util/Logging.h"

#include <mutex>

namespace silex {
namespace core {

static std::map<std::string, std::unique_ptr<Registry>> g_singletons;
static std::mutex g_singletonMutex;

Registry& Registry::instance(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_singletonMutex);
    auto it = g_singletons.find(key);
    if (it == g_singletons.end()) {
        auto [inserted, _] = g_singletons.emplace(key, std::make_unique<Registry>(key));
        return *inserted->second;
    }
    return *it->second;
}

Registry::Registry(const std::string& name)
    : m_name(name) {
    // Register built-in C++ functors and segmenters
    registerBuiltins(*this);
}

const std::string& Registry::name() const {
    return m_name;
}

// MARK: Functors

void Registry::registerFunctor(const SilexFunctorInfo& info, FunctorFactory factory) {
    auto logger = getLogger(LoggerNames::Main);
    const auto& uid = info.uid;

    auto it = m_functorInfo.find(uid);
    if (it != m_functorInfo.end()) {
        logger->debug("Functor '{}' metadata updated", uid);
    }

    m_functorInfo[uid] = info;

    // Don't overwrite a non-null factory with null
    if (factory || m_functorFactories.find(uid) == m_functorFactories.end()) {
        m_functorFactories[uid] = std::move(factory);
    }
}

std::optional<SilexFunctorInfo> Registry::getFunctorInfo(const std::string& name) const {
    // Try exact UID match
    auto it = m_functorInfo.find(name);
    if (it != m_functorInfo.end()) {
        return it->second;
    }

    if (name.find('.') != std::string::npos) {
        // Handle module.name or package.alias format
        auto dotPos = name.rfind('.');
        std::string prefix = name.substr(0, dotPos);
        std::string suffix = name.substr(dotPos + 1);

        for (const auto& [uid, info] : m_functorInfo) {
            bool nameMatch = (info.name == suffix);
            if (!nameMatch) {
                for (const auto& alias : info.aliases) {
                    if (alias == suffix) {
                        nameMatch = true;
                        break;
                    }
                }
            }
            if (!nameMatch) continue;

            if (info.module == prefix || info.package == prefix) {
                return info;
            }
        }
    } else {
        // Handle simple name or alias lookup
        for (const auto& [uid, info] : m_functorInfo) {
            if (info.name == name) return info;
            for (const auto& alias : info.aliases) {
                if (alias == name) return info;
            }
        }
    }

    return std::nullopt;
}

std::shared_ptr<IFunctor> Registry::getFunctor(const std::string& uid) {
    // Check cache first
    auto cacheIt = m_functorCache.find(uid);
    if (cacheIt != m_functorCache.end()) {
        return cacheIt->second;
    }

    // Try to find by UID or alias
    std::string resolvedUid = uid;
    auto infoOpt = getFunctorInfo(uid);
    if (infoOpt) {
        resolvedUid = infoOpt->uid;
        // Check cache with resolved UID
        cacheIt = m_functorCache.find(resolvedUid);
        if (cacheIt != m_functorCache.end()) {
            return cacheIt->second;
        }
    }

    // Create from factory
    auto factoryIt = m_functorFactories.find(resolvedUid);
    if (factoryIt == m_functorFactories.end()) {
        auto logger = getLogger(LoggerNames::Main);
        logger->warn("No factory for functor: {}", uid);
        return nullptr;
    }

    if (!factoryIt->second) {
        auto logger = getLogger(LoggerNames::Main);
        logger->error("Null factory for functor: {} (resolved: {})", uid, resolvedUid);
        return nullptr;
    }

    auto instance = factoryIt->second();
    m_functorCache[resolvedUid] = instance;
    return instance;
}

std::vector<std::string> Registry::getAllFunctorUids() const {
    std::vector<std::string> uids;
    uids.reserve(m_functorInfo.size());
    for (const auto& [uid, _] : m_functorInfo) {
        uids.push_back(uid);
    }
    return uids;
}

// MARK: Segmenters

void Registry::registerSegmenter(const ExternalResource& info, SegmenterFactory factory) {
    auto logger = getLogger(LoggerNames::Main);
    const auto& uid = info.uid;

    auto it = m_segmenterInfo.find(uid);
    if (it != m_segmenterInfo.end()) {
        logger->debug("Segmenter '{}' metadata updated", uid);
    }

    m_segmenterInfo[uid] = info;

    // Don't overwrite a non-null factory with null
    if (factory || m_segmenterFactories.find(uid) == m_segmenterFactories.end()) {
        m_segmenterFactories[uid] = std::move(factory);
    }
}

std::optional<ExternalResource> Registry::getSegmenterInfo(const std::string& name) const {
    auto it = m_segmenterInfo.find(name);
    if (it != m_segmenterInfo.end()) {
        return it->second;
    }

    if (name.find('.') != std::string::npos) {
        auto dotPos = name.rfind('.');
        std::string prefix = name.substr(0, dotPos);
        std::string suffix = name.substr(dotPos + 1);

        for (const auto& [uid, info] : m_segmenterInfo) {
            if (info.name != suffix) continue;
            if (info.module == prefix || info.package == prefix) {
                return info;
            }
        }
    } else {
        for (const auto& [uid, info] : m_segmenterInfo) {
            if (info.name == name) return info;
        }
    }

    return std::nullopt;
}

std::shared_ptr<ISegmenter> Registry::getSegmenter(const std::string& uid) {
    auto cacheIt = m_segmenterCache.find(uid);
    if (cacheIt != m_segmenterCache.end()) {
        return cacheIt->second;
    }

    std::string resolvedUid = uid;
    auto infoOpt = getSegmenterInfo(uid);
    if (infoOpt) {
        resolvedUid = infoOpt->uid;
        cacheIt = m_segmenterCache.find(resolvedUid);
        if (cacheIt != m_segmenterCache.end()) {
            return cacheIt->second;
        }
    }

    auto factoryIt = m_segmenterFactories.find(resolvedUid);
    if (factoryIt == m_segmenterFactories.end()) {
        auto logger = getLogger(LoggerNames::Main);
        logger->warn("No factory for segmenter: {}", uid);
        return nullptr;
    }

    if (!factoryIt->second) {
        auto logger = getLogger(LoggerNames::Main);
        logger->error("Null factory for segmenter: {} (resolved: {})", uid, resolvedUid);
        return nullptr;
    }

    auto instance = factoryIt->second();
    m_segmenterCache[resolvedUid] = instance;
    return instance;
}

// MARK: Schemas

void Registry::registerSchema(const SilexSchemaInfo& info) {
    auto logger = getLogger(LoggerNames::Main);
    const auto& uid = info.uid;

    auto it = m_schemaInfo.find(uid);
    if (it != m_schemaInfo.end()) {
        logger->warn("Schema '{}' is being overwritten with new metadata", uid);
    }

    m_schemaInfo[uid] = info;
}

void Registry::cacheSchema(const std::string& uid, std::shared_ptr<SilexSchema> schema) {
    m_schemaCache[uid] = std::move(schema);
}

std::optional<SilexSchemaInfo> Registry::getSchemaInfo(const std::string& uid) const {
    auto it = m_schemaInfo.find(uid);
    if (it != m_schemaInfo.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::shared_ptr<SilexSchema> Registry::getSchema(const std::string& uid) const {
    auto it = m_schemaCache.find(uid);
    if (it != m_schemaCache.end()) {
        return it->second;
    }
    return nullptr;
}

// MARK: Lifecycle

void Registry::clear() {
    clearCache();
    m_functorInfo.clear();
    m_functorFactories.clear();
    m_schemaInfo.clear();
    m_segmenterInfo.clear();
    m_segmenterFactories.clear();
}

void Registry::clearCache() {
    m_functorCache.clear();
    m_schemaCache.clear();
    m_segmenterCache.clear();
}

} // namespace core
} // namespace silex
