// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file module.cpp
/// @brief pybind11 module definition for silex Python extension.

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;

// Forward declarations
void bindConstants(py::module_& m);
void bindStructs(py::module_& m);
void bindInterfaces(py::module_& m);
void bindResolvers(py::module_& m);
void bindSegmenters(py::module_& m);
void bindFunctors(py::module_& m);
void bindCore(py::module_& m);

PYBIND11_MODULE(silex, m) {
    m.doc() = "Schema-driven path and URI resolver bindings for the Silex C++ core.";

    // Root module: constants, enums, structs
    bindConstants(m);
    bindStructs(m);

    // Submodules
    auto interfaces = m.def_submodule("interfaces", "Abstract interfaces");
    bindInterfaces(interfaces);

    auto resolvers = m.def_submodule("resolvers", "Resolver implementations");
    bindResolvers(resolvers);

    auto segmenters_mod = m.def_submodule("segmenters", "Segmenter implementations");
    bindSegmenters(segmenters_mod);

    auto functors_mod = m.def_submodule("functors", "Functor implementations");
    bindFunctors(functors_mod);

    auto core = m.def_submodule("core", "Core components: Registry, ExpressionParser, etc.");
    bindCore(core);

    // Bubble up commonly-used API to root
    m.attr("GenericResolver") = resolvers.attr("GenericResolver");
    m.attr("set_verbosity") = resolvers.attr("set_verbosity");

    auto jsonModule = py::module_::import("json");
    auto jsonEncoder = jsonModule.attr("JSONEncoder");
    if (!py::hasattr(jsonEncoder, "_silex_box_default")) {
        auto boxType = py::reinterpret_borrow<py::object>(m.attr("Box"));
        jsonEncoder.attr("_silex_box_default") = jsonEncoder.attr("default");
        jsonEncoder.attr("default") = py::cpp_function(
            [jsonEncoder, boxType](py::object self, py::object obj) -> py::object {
                if (py::isinstance(obj, boxType)) {
                    return obj.attr("to_dict")();
                }
                return jsonEncoder.attr("_silex_box_default")(self, obj);
            }, py::is_method(jsonEncoder));
    }

    m.def("set_log_level", [](const std::string& level) {
        if (level == "debug") spdlog::set_level(spdlog::level::debug);
        else if (level == "info") spdlog::set_level(spdlog::level::info);
        else if (level == "warn") spdlog::set_level(spdlog::level::warn);
        else if (level == "error") spdlog::set_level(spdlog::level::err);
        else if (level == "off") spdlog::set_level(spdlog::level::off);
    }, py::arg("level"), "Set the spdlog log level (debug/info/warn/error/off)");
}
