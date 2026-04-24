// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file Box.h
/// @brief Typed accessor for nested ContextMap values with dot-notation paths and defaults.

#pragma once

#include <silex/structs.h>

#include <any>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace silex {

/// Typed read-only accessor for nested ContextMap values.
///
/// Provides convenient dot-notation key lookup with typed defaults,
/// similar to Python's Box for attribute-style dict access.
///
/// @code
///   Box box(result.context);
///   auto project = box.get<std::string>("context.project", "UNK");
///   auto version = box.get<int>("entity.fragment.version", 0);
///   bool hasTags = box.has("tags");
/// @endcode
class Box {
public:
    /// Construct an empty Box.
    Box() = default;

    /// Construct a Box wrapping an existing ContextMap.
    explicit Box(const ContextMap& data) : m_data(data) {}

    /// Construct a Box by moving an existing ContextMap.
    explicit Box(ContextMap&& data) : m_data(std::move(data)) {}

    /// Get a typed value at a dot-separated key path, or the default if missing/wrong type.
    template <typename T>
    T get(const std::string& keyPath, const T& defaultValue = T{}) const {
        auto val = rawGet(keyPath);
        if (!val.has_value()) return defaultValue;
        if (auto* typed = std::any_cast<T>(&*val)) {
            return *typed;
        }
        return defaultValue;
    }

    /// Get a nested Box at a dot-separated key path.
    Box box(const std::string& keyPath) const {
        auto val = rawGet(keyPath);
        if (!val.has_value()) return Box{};
        if (auto* nested = std::any_cast<ContextMap>(&*val)) {
            return Box{*nested};
        }
        return Box{};
    }

    /// Get a list of typed values at a dot-separated key path.
    template <typename T>
    std::vector<T> list(const std::string& keyPath) const {
        std::vector<T> result;
        auto val = rawGet(keyPath);
        if (!val.has_value()) return result;
        if (auto* vec = std::any_cast<std::vector<std::any>>(&*val)) {
            for (const auto& item : *vec) {
                if (auto* typed = std::any_cast<T>(&item)) {
                    result.push_back(*typed);
                }
            }
        }
        return result;
    }

    /// Check whether a key path exists.
    bool has(const std::string& keyPath) const {
        return rawGet(keyPath).has_value();
    }

    /// Access the underlying ContextMap.
    const ContextMap& data() const { return m_data; }

    /// Mutable access to the underlying ContextMap.
    ContextMap& data() { return m_data; }

    /// Set a value at a dot-separated key path.
    template <typename T>
    void set(const std::string& keyPath, const T& value) {
        setNested(m_data, keyPath, std::any(value));
    }

private:
    ContextMap m_data;

    /// Low-level dot-path lookup that returns std::optional<std::any>.
    std::optional<std::any> rawGet(const std::string& keyPath) const {
        auto keys = splitDots(keyPath);
        const ContextMap* current = &m_data;

        for (size_t i = 0; i < keys.size(); ++i) {
            auto it = current->find(keys[i]);
            if (it == current->end()) return std::nullopt;

            if (i + 1 < keys.size()) {
                auto* nested = std::any_cast<ContextMap>(&it->second);
                if (!nested) return std::nullopt;
                current = nested;
            } else {
                return it->second;
            }
        }
        return std::nullopt;
    }

    /// Split a string on '.'.
    static std::vector<std::string> splitDots(const std::string& s) {
        std::vector<std::string> parts;
        std::istringstream stream(s);
        std::string part;
        while (std::getline(stream, part, '.')) {
            parts.push_back(part);
        }
        return parts;
    }

    /// Set a nested value by dot-path.
    static void setNested(ContextMap& m, const std::string& keyPath, const std::any& value) {
        auto keys = splitDots(keyPath);
        ContextMap* current = &m;
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            auto it = current->find(keys[i]);
            if (it == current->end()) {
                (*current)[keys[i]] = ContextMap{};
                current = std::any_cast<ContextMap>(&(*current)[keys[i]]);
            } else {
                auto* nested = std::any_cast<ContextMap>(&it->second);
                if (!nested) {
                    it->second = ContextMap{};
                    nested = std::any_cast<ContextMap>(&it->second);
                }
                current = nested;
            }
        }
        (*current)[keys.back()] = value;
    }
};

} // namespace silex
