// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_helpers.h
/// @brief Shared conversion utilities for pybind11 bindings.

#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <silex/Box.h>
#include <silex/structs.h>

#include <any>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace py = pybind11;

namespace silex_bind {

/// Convert a Python object to std::any.
inline std::any pyToAny(py::handle obj) {
    if (obj.is_none()) return std::any{};
    if (py::isinstance<py::bool_>(obj)) return std::any(obj.cast<bool>());
    if (py::isinstance<py::int_>(obj)) return std::any(obj.cast<int>());
    if (py::isinstance<py::str>(obj)) return std::any(obj.cast<std::string>());
    if (py::isinstance<py::float_>(obj)) return std::any(obj.cast<double>());
    if (py::isinstance<silex::PlaceholderValue>(obj))
        return std::any(obj.cast<silex::PlaceholderValue>());
    if (py::isinstance<silex::Box>(obj)) {
        return std::any(obj.cast<silex::Box>().data());
    }
    if (py::isinstance<py::dict>(obj)) {
        silex::ContextMap map;
        for (auto item : obj.cast<py::dict>()) {
            map[item.first.cast<std::string>()] = pyToAny(item.second);
        }
        return std::any(map);
    }
    if (py::isinstance<py::list>(obj)) {
        std::vector<std::any> vec;
        for (auto item : obj.cast<py::list>()) {
            vec.push_back(pyToAny(item));
        }
        return std::any(vec);
    }
    return std::any(obj.cast<std::string>());
}

/// Convert std::any to Python object, returning Box for nested ContextMaps.
inline py::object anyToPy(const std::any& val) {
    if (!val.has_value()) return py::none();
    if (auto* b = std::any_cast<bool>(&val)) return py::cast(*b);
    if (auto* i = std::any_cast<int>(&val)) return py::cast(*i);
    if (auto* s = std::any_cast<std::string>(&val)) return py::cast(*s);
    if (auto* d = std::any_cast<double>(&val)) return py::cast(*d);
    if (auto* ph = std::any_cast<silex::PlaceholderValue>(&val)) return py::cast(*ph);
    if (auto* m = std::any_cast<silex::ContextMap>(&val)) {
        return py::cast(silex::Box(*m));
    }
    if (auto* vs = std::any_cast<std::vector<std::string>>(&val)) {
        py::list result;
        for (const auto& item : *vs) result.append(py::cast(item));
        return result;
    }
    if (auto* ms = std::any_cast<std::map<std::string, std::string>>(&val)) {
        py::dict result;
        for (const auto& [k, v] : *ms) result[py::cast(k)] = py::cast(v);
        return result;
    }
    if (auto* v = std::any_cast<std::vector<std::any>>(&val)) {
        py::list result;
        for (const auto& item : *v) {
            result.append(anyToPy(item));
        }
        return result;
    }
    if (auto* g = std::any_cast<silex::SilexExpressionGraph>(&val)) {
        return py::cast(*g);
    }
    return py::none();
}

/// Convert a py::dict or Box to ContextMap.
inline silex::ContextMap toContextMap(py::handle obj) {
    if (py::isinstance<silex::Box>(obj)) {
        return obj.cast<silex::Box>().data();
    }
    silex::ContextMap map;
    for (auto item : obj.cast<py::dict>()) {
        map[item.first.cast<std::string>()] = pyToAny(item.second);
    }
    return map;
}

/// Convert ContextMap to Python dict.
inline py::dict contextMapToDict(const silex::ContextMap& map) {
    py::dict result;
    for (const auto& [k, v] : map) {
        result[py::cast(k)] = anyToPy(v);
    }
    return result;
}

/// Convert optional<string> to Python str or None.
inline py::object optStrToPy(const std::optional<std::string>& val) {
    return val ? py::cast(*val) : py::none();
}

/// Convert Python object to optional<string>.
inline std::optional<std::string> pyToOptStr(py::handle obj) {
    if (obj.is_none()) return std::nullopt;
    return obj.cast<std::string>();
}

/// Parse optional endpoint/schema arguments from Python.
inline std::optional<std::vector<std::string>> parseOptionalStringList(py::object obj) {
    if (obj.is_none()) return std::nullopt;
    if (py::isinstance<py::str>(obj)) {
        return std::vector<std::string>{obj.cast<std::string>()};
    }
    return obj.cast<std::vector<std::string>>();
}

/// Convert FunctorInput variant to Python object.
inline py::object functorInputToPy(const silex::FunctorInput& input) {
    return std::visit([](auto&& v) -> py::object { return py::cast(v); }, input);
}

/// Convert Python object to FunctorInput variant.
inline silex::FunctorInput pyToFunctorInput(py::handle obj) {
    if (py::isinstance<py::bool_>(obj)) return silex::FunctorInput(obj.cast<bool>());
    if (py::isinstance<py::int_>(obj)) return silex::FunctorInput(obj.cast<int>());
    if (py::isinstance<py::float_>(obj)) return silex::FunctorInput(obj.cast<double>());
    return silex::FunctorInput(obj.cast<std::string>());
}

} // namespace silex_bind
