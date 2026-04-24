/// @file SchemaValidator.cpp
/// @brief Implementation of JSON schema validation for .silex files.

#include "SchemaValidator.h"
#include "util/Logging.h"

#include <nlohmann/json.hpp>
#include <json/value.h>

#include <fstream>
#include <sstream>

namespace silex {
namespace core {

// Helper to convert Json::Value (JsonCpp) to nlohmann::json
static nlohmann::json jsoncppToNlohmann(const Json::Value& jval) {
    switch (jval.type()) {
        case Json::nullValue:
            return nullptr;
        case Json::intValue:
            return jval.asInt64();
        case Json::uintValue:
            return jval.asUInt64();
        case Json::realValue:
            return jval.asDouble();
        case Json::stringValue:
            return jval.asString();
        case Json::booleanValue:
            return jval.asBool();
        case Json::arrayValue: {
            auto arr = nlohmann::json::array();
            for (Json::ArrayIndex i = 0; i < jval.size(); ++i) {
                arr.push_back(jsoncppToNlohmann(jval[i]));
            }
            return arr;
        }
        case Json::objectValue: {
            auto obj = nlohmann::json::object();
            for (const auto& key : jval.getMemberNames()) {
                obj[key] = jsoncppToNlohmann(jval[key]);
            }
            return obj;
        }
    }
    return nullptr;
}

bool SchemaValidator::loadSchema(const std::string& schemaPath) {
    auto logger = getLogger(LoggerNames::SchemaLoader);

    std::ifstream file(schemaPath);
    if (!file.is_open()) {
        logger->error("Could not open schema file: {}", schemaPath);
        return false;
    }

    try {
        m_schemaDoc = nlohmann::json::parse(file);
        m_loaded = true;
        logger->info("Loaded JSON Schema from: {}", schemaPath);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        logger->error("Failed to parse schema file {}: {}", schemaPath, e.what());
        return false;
    }
}

bool SchemaValidator::validate(const Json::Value& document, std::string& errorMessage) const {
    if (!m_loaded) {
        errorMessage = "No schema loaded for validation";
        return false;
    }

    try {
        // Convert document to nlohmann::json for validation
        auto nlDoc = jsoncppToNlohmann(document);

        // Basic structural validation: check required top-level fields
        if (!nlDoc.is_object()) {
            errorMessage = "Document must be a JSON object";
            return false;
        }

        // Check for required 'uid' field
        if (!nlDoc.contains("uid") || !nlDoc["uid"].is_string()) {
            errorMessage = "Document must have a string 'uid' field";
            return false;
        }

        // Validate against schema structure
        const auto& schemaDef = m_schemaDoc;
        if (schemaDef.contains("required") && schemaDef["required"].is_array()) {
            for (const auto& req : schemaDef["required"]) {
                if (req.is_string() && !nlDoc.contains(req.get<std::string>())) {
                    errorMessage = "Missing required field: " + req.get<std::string>();
                    return false;
                }
            }
        }

        // Validate property types if schema defines them
        if (schemaDef.contains("properties") && schemaDef["properties"].is_object()) {
            for (auto it = schemaDef["properties"].begin(); it != schemaDef["properties"].end(); ++it) {
                const auto& propName = it.key();
                const auto& propSchema = it.value();

                if (!nlDoc.contains(propName)) continue;

                if (propSchema.contains("type")) {
                    const auto& expectedType = propSchema["type"].get<std::string>();
                    const auto& actual = nlDoc[propName];

                    bool typeOk = false;
                    if (expectedType == "string") typeOk = actual.is_string();
                    else if (expectedType == "object") typeOk = actual.is_object();
                    else if (expectedType == "array") typeOk = actual.is_array();
                    else if (expectedType == "boolean") typeOk = actual.is_boolean();
                    else if (expectedType == "number" || expectedType == "integer") typeOk = actual.is_number();
                    else typeOk = true; // unknown type, pass

                    if (!typeOk) {
                        errorMessage = "Field '" + propName + "' should be of type " + expectedType;
                        return false;
                    }
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        errorMessage = std::string("Validation error: ") + e.what();
        return false;
    }
}

bool SchemaValidator::isLoaded() const {
    return m_loaded;
}

} // namespace core
} // namespace silex
