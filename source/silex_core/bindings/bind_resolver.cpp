// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_resolver.cpp
/// @brief Python bindings for GenericResolver.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <filesystem>

#include <silex/Box.h>
#include <silex/GenericResolver.h>
#include "util/Logging.h"
#include <silex/structs.h>
#include "bind_helpers.h"

namespace py = pybind11;
using namespace silex;

// MARK: Bindings

void bindResolvers(py::module_& m) {
    // set_verbosity (enum)
    m.def("set_verbosity", [](Verbosity v) {
        silex::setVerbosity(v);
    }, py::arg("verbosity"),
    "Set the global Silex logging verbosity level using a `Verbosity` value.");

    // set_verbosity (int overload)
    m.def("set_verbosity", [](int v) {
        silex::setVerbosity(static_cast<Verbosity>(v));
    }, py::arg("verbosity"),
    "Set the global Silex logging verbosity level from an integer value.");

    // GenericResolver
    py::class_<resolvers::GenericResolver>(
        m,
        "GenericResolver",
        "Primary resolver for converting between paths, URIs, and structured context.")
        .def(py::init([](py::args args, py::kwargs kwargs) {
            SilexParseOptions options;
            std::string schema;
            std::string configId = "default";

            // Accept SilexParseOptions as first positional arg.
            if (args.size() >= 1 && py::isinstance<SilexParseOptions>(args[0])) {
                options = args[0].cast<SilexParseOptions>();
                if (options.schema.has_value() && !options.schema->empty()) {
                    schema = options.schema->front();
                }
            }

            // Accept a SilexParseOptions object via 'options' kwarg.
            if (kwargs.contains("options")) {
                auto optObj = kwargs["options"];
                if (!optObj.is_none()) {
                    options = optObj.cast<SilexParseOptions>();
                    if (options.schema.has_value() && !options.schema->empty()) {
                        schema = options.schema->front();
                    }
                }
            }

            // Individual kwargs override options fields.
            if (kwargs.contains("include_deprecated")) {
                options.includeDeprecated = kwargs["include_deprecated"].cast<bool>();
            }
            if (kwargs.contains("schema")) {
                schema = kwargs["schema"].cast<std::string>();
            }
            if (kwargs.contains("config_id")) {
                configId = kwargs["config_id"].cast<std::string>();
            }
            if (kwargs.contains("segment_limit")) {
                auto limit = kwargs["segment_limit"];
                if (!limit.is_none()) {
                    auto pair = limit.cast<py::tuple>();
                    options.segmentLimit = std::make_pair(
                        pair[0].cast<int>(), pair[1].cast<int>());
                }
            }

            return std::make_unique<resolvers::GenericResolver>(options, schema, configId);
        }))
        .def("context_from_path",
            [](resolvers::GenericResolver& self, const std::string& path,
               py::object endpoint, py::object schema) {
                auto ep = silex_bind::parseOptionalStringList(endpoint);
                auto sc = silex_bind::parseOptionalStringList(schema);

                SilexContextResolve result;
                {
                    py::gil_scoped_release release;
                    result = self.contextFromPath(path, ep, sc);
                }
                if (!result.sourcePath) {
                    result.sourcePath = path;
                }
                return result;
            },
            py::arg("path"),
            py::arg("endpoint") = py::none(),
            py::arg("schema") = py::none(),
            "Resolve a filesystem path or URI into a `SilexContextResolve` result.")
        .def("path_from_context",
            [](resolvers::GenericResolver& self, py::object context,
               py::object endpoint, py::object schema, bool includeChildren) {
                auto ctx = silex_bind::toContextMap(context);
                auto ep = silex_bind::parseOptionalStringList(endpoint);
                auto sc = silex_bind::parseOptionalStringList(schema);

                SilexPathResolve result;
                {
                    py::gil_scoped_release release;
                    result = self.pathFromContext(ctx, ep, sc, includeChildren);
                }
                return result;
            },
            py::arg("context"),
            py::arg("endpoint") = py::none(),
            py::arg("schema") = py::none(),
            py::arg("include_children") = false,
            "Generate a path from context and return a `SilexPathResolve` result.")
        .def("available_templates",
            [](resolvers::GenericResolver& self, py::object schema) {
                auto sc = silex_bind::parseOptionalStringList(schema);
                return self.availableTemplates(sc);
            },
            py::arg("schema") = py::none(),
            "List template names visible to the resolver, optionally filtered by schema.")
        .def("parse_template_value",
            [](resolvers::GenericResolver& self, const std::string& value,
               const std::string& templateName, py::object context, py::object schema) {
                std::optional<ContextMap> ctx;
                if (!context.is_none()) {
                    ctx = silex_bind::toContextMap(context);
                }
                auto sc = silex_bind::parseOptionalStringList(schema);
                auto result = self.parseTemplateValue(value, templateName, ctx, sc);
                return silex_bind::contextMapToDict(result);
            },
            py::arg("value"),
            py::arg("template_name"),
            py::arg("context") = py::none(),
            py::arg("schema") = py::none(),
            "Extract context values from a single template-formatted value.")
        .def("format_template_value",
            [](resolvers::GenericResolver& self, py::object context,
               const std::string& templateName, py::object schema) {
                auto ctx = silex_bind::toContextMap(context);
                auto sc = silex_bind::parseOptionalStringList(schema);
                return self.formatTemplateValue(ctx, templateName, sc);
            },
            py::arg("context"),
            py::arg("template_name"),
            py::arg("schema") = py::none(),
            "Render a template-formatted value from context data.")
        .def("schema_from_path",
            [](resolvers::GenericResolver& self, const std::string& path) -> py::object {
                auto result = self.schemaFromPath(path);
                if (result) return py::cast(*result);
                return py::none();
            },
            py::arg("path"),
            "Return the schema UID that best matches the supplied path, if any.");
}
