// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_constants.cpp
/// @brief Python bindings for Silex enums and constants.

#include <pybind11/pybind11.h>
#include <silex/constants.h>

namespace py = pybind11;

void bindConstants(py::module_& m) {
    // Verbosity enum (PascalCase canonical, UPPER_CASE aliases for backward compat)
    py::enum_<silex::Verbosity>(m, "Verbosity", "Logging verbosity levels used by Silex.")
        .value("Quiet", silex::Verbosity::Quiet)
        .value("Info", silex::Verbosity::Info)
        .value("Flow", silex::Verbosity::Flow)
        .value("Detail", silex::Verbosity::Detail)
        .value("Trace", silex::Verbosity::Trace)
        .value("QUIET", silex::Verbosity::Quiet)
        .value("INFO", silex::Verbosity::Info)
        .value("FLOW", silex::Verbosity::Flow)
        .value("DETAIL", silex::Verbosity::Detail)
        .value("TRACE", silex::Verbosity::Trace)
        .export_values();

    // SegmentFlags enum (arithmetic for bitwise ops, UPPER names match Python convention)
    auto segFlags = py::enum_<silex::SegmentFlags>(
        m,
        "SegmentFlags",
        "Bitflags that describe lifecycle and traversal behavior for schema segments.",
        py::arithmetic())
        .value("NONE", silex::SegmentFlags::None)
        .value("DEPRECATED", silex::SegmentFlags::Deprecated)
        .value("READONLY", silex::SegmentFlags::ReadOnly)
        .value("PROMOTE", silex::SegmentFlags::Promote)
        .value("OMIT", silex::SegmentFlags::Omit)
        .value("INTERMEDIATE", silex::SegmentFlags::Intermediate)
        .value("HAS_PARTITIONS", silex::SegmentFlags::HasPartitions)
        .value("FALLBACK", silex::SegmentFlags::Fallback)
        .export_values();

    // Add explicit bitwise operators for SegmentFlags
    segFlags.def("__and__", [](silex::SegmentFlags a, silex::SegmentFlags b) {
        return a & b;
    }, py::is_operator());
    segFlags.def("__or__", [](silex::SegmentFlags a, silex::SegmentFlags b) {
        return a | b;
    }, py::is_operator());
    segFlags.def("__bool__", [](silex::SegmentFlags a) {
        return static_cast<uint32_t>(a) != 0;
    });

    // ResolverStatus enum (arithmetic for bitwise ops, UPPER names match Python convention)
    auto resolverStatus = py::enum_<silex::ResolverStatus>(
        m,
        "ResolverStatus",
        "Status flags describing the outcome of a resolver operation.",
        py::arithmetic())
        .value("NONE", silex::ResolverStatus::None)
        .value("SUCCESS", silex::ResolverStatus::Success)
        .value("MISSING_TARGETS", silex::ResolverStatus::MissingTargets)
        .value("PARTIAL", silex::ResolverStatus::Partial)
        .value("AMBIGUOUS", silex::ResolverStatus::Ambiguous)
        .value("ERROR", silex::ResolverStatus::Error)
        .export_values();

    resolverStatus.def("__or__", [](silex::ResolverStatus a, silex::ResolverStatus b) {
        return a | b;
    }, py::is_operator());
    resolverStatus.def("__and__", [](silex::ResolverStatus a, silex::ResolverStatus b) {
        return a & b;
    }, py::is_operator());
    resolverStatus.def("__int__", [](silex::ResolverStatus s) {
        return static_cast<uint32_t>(s);
    });

    // ExpressionMode enum
    py::enum_<silex::ExpressionMode>(m, "ExpressionMode", "Direction of expression evaluation.")
        .value("Parse", silex::ExpressionMode::Parse)
        .value("Format", silex::ExpressionMode::Format)
        .value("PARSE", silex::ExpressionMode::Parse)
        .value("FORMAT", silex::ExpressionMode::Format)
        .export_values();

    // Language enum
    py::enum_<silex::Language>(m, "Language", "Implementation language for externally registered resources.")
        .value("Python", silex::Language::Python)
        .value("JavaScript", silex::Language::JavaScript)
        .value("Other", silex::Language::Other)
        .value("PYTHON", silex::Language::Python)
        .value("JAVASCRIPT", silex::Language::JavaScript)
        .value("OTHER", silex::Language::Other)
        .export_values();

    // FunctorParams constants
    auto fp = m.def_submodule("FunctorParams", "Functor output parameter constants");
    fp.attr("OPTIONAL") = silex::FunctorParams::Optional;
    fp.attr("GREEDY") = silex::FunctorParams::Greedy;
    fp.attr("FIRST_INDEX") = silex::FunctorParams::FirstIndex;
    fp.attr("LAST_INDEX") = silex::FunctorParams::LastIndex;
}
