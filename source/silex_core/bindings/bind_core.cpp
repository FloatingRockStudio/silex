// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_core.cpp
/// @brief Python bindings for core components: Registry, ExpressionParser, ExpressionEvaluator, FileSchemaLoader.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <silex/interfaces/IFunctor.h>
#include <silex/structs.h>
#include "bind_helpers.h"
#include "registry/Registry.h"
#include "expression/ExpressionParser.h"
#include "expression/ExpressionEvaluator.h"
#include "schema/FileSchemaLoader.h"

#include <json/value.h>

namespace py = pybind11;

namespace {
/// Convert Json::Value to Python object recursively.
py::object jsonToPy(const Json::Value& val) {
    switch (val.type()) {
        case Json::nullValue: return py::none();
        case Json::intValue: return py::cast(val.asInt());
        case Json::uintValue: return py::cast(val.asUInt());
        case Json::realValue: return py::cast(val.asDouble());
        case Json::stringValue: return py::cast(val.asString());
        case Json::booleanValue: return py::cast(val.asBool());
        case Json::arrayValue: {
            py::list result;
            for (Json::ArrayIndex i = 0; i < val.size(); ++i)
                result.append(jsonToPy(val[i]));
            return result;
        }
        case Json::objectValue: {
            py::dict result;
            for (const auto& key : val.getMemberNames())
                result[py::cast(key)] = jsonToPy(val[key]);
            return result;
        }
        default: return py::none();
    }
}
} // namespace
using namespace silex;

void bindCore(py::module_& m) {

    // MARK: Registry

    py::class_<core::Registry>(m, "Registry", py::dynamic_attr())
        .def(py::init<const std::string&>(), py::arg("name") = "default")
        .def_static("instance", &core::Registry::instance,
            py::arg("key") = "silex",
            py::return_value_policy::reference,
            "Get or create a singleton instance of the registry.")
        .def("name", &core::Registry::name,
            "Get the registry name.")
        .def("register_functor",
            [](core::Registry& self, const SilexFunctorInfo& info, std::shared_ptr<IFunctor> functor) {
                self.registerFunctor(info, [functor]() { return functor; });
            },
            py::arg("info"), py::arg("functor"),
            "Register a functor with its metadata and instance.")
        .def("register_functor",
            [](core::Registry& self, const SilexFunctorInfo& info) {
                self.registerFunctor(info, nullptr);
            },
            py::arg("info"),
            "Register functor metadata without an instance.")
        .def("get_functor_info",
            [](const core::Registry& self, const std::string& name) -> py::object {
                auto info = self.getFunctorInfo(name);
                if (info) return py::cast(*info);
                return py::none();
            },
            py::arg("name"),
            "Get functor info by name, alias, or UID.")
        .def("get_functor",
            [](core::Registry& self, const std::string& uid) -> py::object {
                auto f = self.getFunctor(uid);
                if (f) return py::cast(f);
                return py::none();
            },
            py::arg("uid"),
            "Get functor instance by UID.")
        .def("get_all_functor_uids", &core::Registry::getAllFunctorUids,
            "Get all registered functor UIDs.")
        .def("register_schema", &core::Registry::registerSchema,
            py::arg("info"),
            "Register a schema with its metadata.")
        .def("get_schema_info",
            [](const core::Registry& self, const std::string& uid) -> py::object {
                auto info = self.getSchemaInfo(uid);
                if (info) return py::cast(*info);
                return py::none();
            },
            py::arg("uid"),
            "Get schema info by UID.")
        .def("get_segmenter_info",
            [](const core::Registry& self, const std::string& name) -> py::object {
                auto info = self.getSegmenterInfo(name);
                if (info) return py::cast(*info);
                return py::none();
            },
            py::arg("name"),
            "Get segmenter info by name or UID.")
        .def("get_schema",
            [](const core::Registry& self, const std::string& uid) -> py::object {
                auto schema = self.getSchema(uid);
                if (schema) return py::cast(schema);
                return py::none();
            },
            py::arg("uid"),
            "Get cached schema by UID.")
        .def("clear", &core::Registry::clear,
            "Clear all registered components and caches.")
        .def("clear_cache", &core::Registry::clearCache,
            "Clear only caches, keeping registrations.");

    // MARK: ExpressionParser

    py::class_<core::ExpressionParser>(m, "ExpressionParser")
        .def(py::init([](core::Registry& registry, py::dict aliases) {
            std::map<std::string, std::string> aliasMap;
            for (auto item : aliases) {
                aliasMap[item.first.cast<std::string>()] = item.second.cast<std::string>();
            }
            return std::make_unique<core::ExpressionParser>(registry, aliasMap);
        }),
            py::arg("registry"),
            py::arg("aliases") = py::dict(),
            py::keep_alive<1, 2>())
        .def_property_readonly("_registry",
            [](core::ExpressionParser& self) -> core::Registry& { return self.registry(); },
            py::return_value_policy::reference)
        .def("parse_expressions", &core::ExpressionParser::parseExpressions,
            py::arg("expressions"),
            "Parse a list of expression strings into an expression graph.");

    // MARK: ExpressionEvaluator

    py::class_<core::ExpressionEvaluator>(m, "ExpressionEvaluator")
        .def(py::init<core::Registry&>(),
            py::arg("registry"),
            py::keep_alive<1, 2>())
        .def_property_readonly("_registry",
            [](core::ExpressionEvaluator& self) -> core::Registry& { return self.registry(); },
            py::return_value_policy::reference)
        .def("evaluate_graph", &core::ExpressionEvaluator::evaluateGraph,
            py::arg("graph"), py::arg("context"), py::arg("config"),
            "Evaluate an expression graph using dependency resolution.")
        .def("evaluate_parse_expression", &core::ExpressionEvaluator::evaluateParseExpression,
            py::arg("expression"), py::arg("inputs"), py::arg("context"), py::arg("config"),
            "Evaluate a parse expression.")
        .def("evaluate_format_expression", &core::ExpressionEvaluator::evaluateFormatExpression,
            py::arg("expression"), py::arg("inputs"), py::arg("context"), py::arg("config"),
            "Evaluate a format expression.")
        .def("evaluate_format_expression",
            [](core::ExpressionEvaluator& self,
               const SilexExpression& expression,
               const FunctorContext& context,
               const SilexConfig& config) {
                // Wrap in a single-expression graph and evaluate
                SilexExpressionGraph tempGraph;
                tempGraph.expressions = {expression};
                tempGraph.mode = ExpressionMode::Format;
                tempGraph.outputs = {"output"};
                auto graphResult = self.evaluateGraph(tempGraph, context, config);
                if (!graphResult.success) {
                    return FormatResult{false, graphResult.message, ""};
                }
                // Extract the formatted output
                auto outIt = graphResult.outputs.find("output");
                if (outIt != graphResult.outputs.end()) {
                    if (auto* s = std::any_cast<std::string>(&outIt->second.value)) {
                        return FormatResult{true, "Success", *s};
                    }
                }
                return FormatResult{false, "No output produced", ""};
            },
            py::arg("expression"), py::arg("context"), py::arg("config"),
            "Evaluate a format expression with inputs resolved from context.");

    // MARK: FileSchemaLoader

    py::class_<core::FileSchemaLoader>(m, "FileSchemaLoader")
        .def(py::init<core::Registry&>(),
            py::arg("registry"),
            py::keep_alive<1, 2>())
        .def("preload", &core::FileSchemaLoader::preload,
            "Preload all .silex files from SILEX_SCHEMA_PATH.")
        .def("match",
            [](const core::FileSchemaLoader& self,
               py::object uidPattern, py::object path, py::object context) {
                std::optional<std::string> uid;
                std::optional<std::string> p;
                std::optional<ContextMap> ctx;
                if (!uidPattern.is_none()) uid = uidPattern.cast<std::string>();
                if (!path.is_none()) p = path.cast<std::string>();
                if (!context.is_none()) ctx = silex_bind::toContextMap(context);
                return self.match(uid, p, ctx);
            },
            py::arg("uid_pattern") = py::none(),
            py::arg("path") = py::none(),
            py::arg("context") = py::none(),
            "Match schemas by UID pattern, path, or context.")
        .def("load_info", &core::FileSchemaLoader::loadInfo,
            py::arg("uid"),
            "Load schema info by UID.")
        .def("load_schema", &core::FileSchemaLoader::loadSchema,
            py::arg("uid"),
            "Load a full Silex schema by UID.")
        .def("available_schema", &core::FileSchemaLoader::availableSchema,
            "Return a list of all available schema UIDs.")
        .def_property_readonly("_available_schemas",
            [](const core::FileSchemaLoader& self) -> py::dict {
                py::dict result;
                for (const auto& uid : self.availableSchema()) {
                    py::dict entry;
                    auto* doc = self.getMergedDoc(uid);
                    if (doc) {
                        entry["merged_data"] = jsonToPy(*doc);
                    }
                    result[py::cast(uid)] = entry;
                }
                return result;
            },
            "Internal: available schemas with merged data.");
}
