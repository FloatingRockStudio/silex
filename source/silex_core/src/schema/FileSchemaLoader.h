#pragma once

/// @file FileSchemaLoader.h
/// @brief Loads .silex JSON5 schema files from disk via SILEX_SCHEMA_PATH.

#include <silex/interfaces/ILoader.h>
#include <silex/structs.h>
#include "SchemaValidator.h"

#include <json/value.h>

#include <filesystem>
#include <map>
#include <string>

namespace silex {
namespace core {

class Registry;

/// Loads Silex schemas from .silex JSON5 files on the search path.
/// Handles inheritance, imports, template resolution, and endpoint extraction.
class FileSchemaLoader : public ILoader {
public:
    /// Construct a loader using the given component registry.
    explicit FileSchemaLoader(Registry& registry);
    ~FileSchemaLoader() override;

    FileSchemaLoader(const FileSchemaLoader&) = delete;
    FileSchemaLoader& operator=(const FileSchemaLoader&) = delete;
    FileSchemaLoader(FileSchemaLoader&&) = delete;
    FileSchemaLoader& operator=(FileSchemaLoader&&) = delete;

    /// Preload all .silex files from SILEX_SCHEMA_PATH environment variable.
    void preload() override;

    /// Match schemas by UID pattern, path, or context.
    std::vector<SilexSchemaInfo> match(
        const std::optional<std::string>& uidPattern = std::nullopt,
        const std::optional<std::string>& path = std::nullopt,
        const std::optional<ContextMap>& context = std::nullopt) const override;

    /// Load schema info by UID.
    SilexSchemaInfo loadInfo(const std::string& uid) const override;

    /// Load a full Silex schema by its unique identifier.
    std::shared_ptr<SilexSchema> loadSchema(const std::string& uid) override;

    /// Return a list of all available schema UIDs.
    std::vector<std::string> availableSchema() const override;

    /// Get the merged document for a given schema UID.
    const Json::Value* getMergedDoc(const std::string& uid) const;

private:
    /// Scan SILEX_SCHEMA_PATH directories for .silex files.
    void discoverFiles();
    /// Resolve inheritance links (segmenter, rootPath, pathPattern) after all files are loaded.
    void resolveInheritance();
    /// Parse a .silex file and extract its schema info.
    void loadFileInfo(const std::string& filePath);
    /// Parse a JSON5 file into a Json::Value document.
    Json::Value parseJson5File(const std::string& filePath);
    /// Resolve and merge imported schema files into the document.
    void processImports(Json::Value& doc, const std::string& filePath);
    /// Locate a schema file by name on the search path.
    std::filesystem::path findSchemaFile(const std::string& name);
    /// Register functors and segmenters declared in a schema document.
    void registerComponents(const Json::Value& doc);
    /// Build a complete SilexSchema from a loaded document.
    SilexSchema buildSchema(const std::string& uid);
    /// Check if a UID matches a glob-style pattern.
    bool matchUidPattern(const std::string& uid, const std::string& pattern) const;

    Registry& m_registry;
    std::map<std::string, std::string> m_uidToPath;
    std::map<std::string, SilexSchemaInfo> m_uidToInfo;
    std::map<std::string, Json::Value> m_uidToDoc;
    SchemaValidator m_validator;
    bool m_preloaded = false;
};

} // namespace core
} // namespace silex
