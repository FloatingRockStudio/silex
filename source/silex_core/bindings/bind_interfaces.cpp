// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_interfaces.cpp
/// @brief Python bindings for Silex abstract interfaces with trampolines.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <silex/interfaces/IFunctor.h>
#include <silex/interfaces/ISegmenter.h>
#include <silex/structs.h>
#include "bind_helpers.h"

namespace py = pybind11;
using namespace silex;

// MARK: Trampolines

/// Trampoline class allowing Python subclasses of IFunctor.
class PyIFunctor : public IFunctor {
public:
    using IFunctor::IFunctor;

    ParseResult parse(
        const std::vector<FunctorInput>& inputs,
        const std::vector<FunctorOutput>& outputs,
        const FunctorContext& context) override {
        PYBIND11_OVERRIDE_PURE(ParseResult, IFunctor, parse, inputs, outputs, context);
    }

    FormatResult format(
        const std::vector<FunctorInput>& inputs,
        const FunctorContext& context) override {
        PYBIND11_OVERRIDE_PURE(FormatResult, IFunctor, format, inputs, context);
    }
};

/// Trampoline class allowing Python subclasses of ISegmenter.
class PyISegmenter : public ISegmenter {
public:
    using ISegmenter::ISegmenter;

    std::string pathPattern() const override {
        PYBIND11_OVERRIDE_PURE(std::string, ISegmenter, pathPattern);
    }

    std::vector<std::string> splitPath(
        const std::string& rootPath, const std::string& path) const override {
        PYBIND11_OVERRIDE_PURE(std::vector<std::string>, ISegmenter, splitPath, rootPath, path);
    }

    std::string joinSegments(
        const std::string& rootPath, const std::vector<std::string>& segments) const override {
        PYBIND11_OVERRIDE_PURE(std::string, ISegmenter, joinSegments, rootPath, segments);
    }

    bool matchesRoot(
        const std::string& rootPath, const std::string& path) const override {
        PYBIND11_OVERRIDE_PURE(bool, ISegmenter, matchesRoot, rootPath, path);
    }
};

// MARK: Bindings

void bindInterfaces(py::module_& m) {
    py::class_<IFunctor, PyIFunctor, std::shared_ptr<IFunctor>>(m, "IFunctor")
        .def(py::init<>())
        .def("parse", [](IFunctor& self, py::list inputs, const std::vector<FunctorOutput>& outputs,
                         const FunctorContext& context) {
            std::vector<FunctorInput> cppInputs;
            for (auto item : inputs) {
                if (item.is_none()) {
                    cppInputs.push_back(FunctorInput(std::string("")));
                } else {
                    cppInputs.push_back(silex_bind::pyToFunctorInput(item));
                }
            }
            return self.parse(cppInputs, outputs, context);
        },
            py::arg("inputs"), py::arg("outputs"), py::arg("context"),
            "Execute parse operation: parse input and produce named outputs.")
        .def("format", [](IFunctor& self, py::list inputs, const FunctorContext& context) {
            std::vector<FunctorInput> cppInputs;
            for (auto item : inputs) {
                if (item.is_none()) {
                    cppInputs.push_back(FunctorInput(std::string("")));
                } else {
                    cppInputs.push_back(silex_bind::pyToFunctorInput(item));
                }
            }
            return self.format(cppInputs, context);
        },
            py::arg("inputs"), py::arg("context"),
            "Execute format operation: take inputs and produce a single string output.");

    py::class_<ISegmenter, PyISegmenter, std::shared_ptr<ISegmenter>>(m, "ISegmenter")
        .def(py::init<>())
        .def("path_pattern", &ISegmenter::pathPattern,
            "Get the regex pattern for matching paths handled by this segmenter.")
        .def("split_path", &ISegmenter::splitPath,
            py::arg("root_path"), py::arg("path"),
            "Split a path into segments relative to a root path.")
        .def("join_segments", &ISegmenter::joinSegments,
            py::arg("root_path"), py::arg("segments"),
            "Join segments into a path relative to a root path.")
        .def("matches_root", &ISegmenter::matchesRoot,
            py::arg("root_path"), py::arg("path"),
            "Check if path matches a root filter pattern.");
}
