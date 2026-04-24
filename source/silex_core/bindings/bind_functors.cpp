// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_functors.cpp
/// @brief Python bindings for concrete functor implementations.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <silex/interfaces/IFunctor.h>
#include "functors/CaseConversionFunctor.h"
#include "functors/CaseSplitFunctor.h"
#include "functors/GlobFunctor.h"
#include "functors/GlobTagFunctor.h"
#include "functors/LexiconFunctor.h"

namespace py = pybind11;
using namespace silex;

void bindFunctors(py::module_& m) {
    py::class_<functors::ConvertLowerCaseFunctor, IFunctor, std::shared_ptr<functors::ConvertLowerCaseFunctor>>(m, "ConvertLowerCaseFunctor")
        .def(py::init<>());

    py::class_<functors::ConvertUpperCaseFunctor, IFunctor, std::shared_ptr<functors::ConvertUpperCaseFunctor>>(m, "ConvertUpperCaseFunctor")
        .def(py::init<>());

    py::class_<functors::ConvertTitleCaseFunctor, IFunctor, std::shared_ptr<functors::ConvertTitleCaseFunctor>>(m, "ConvertTitleCaseFunctor")
        .def(py::init<>());

    py::class_<functors::SplitCamelCaseFunctor, IFunctor, std::shared_ptr<functors::SplitCamelCaseFunctor>>(m, "SplitCamelCaseFunctor")
        .def(py::init<>());

    py::class_<functors::SplitSnakeCaseFunctor, IFunctor, std::shared_ptr<functors::SplitSnakeCaseFunctor>>(m, "SplitSnakeCaseFunctor")
        .def(py::init<>());

    py::class_<functors::GlobFunctor, IFunctor, std::shared_ptr<functors::GlobFunctor>>(m, "GlobFunctor")
        .def(py::init<>());

    py::class_<functors::GlobTagFunctor, IFunctor, std::shared_ptr<functors::GlobTagFunctor>>(m, "GlobTagFunctor")
        .def(py::init<>());

    py::class_<functors::LexiconFunctor, IFunctor, std::shared_ptr<functors::LexiconFunctor>>(m, "LexiconFunctor")
        .def(py::init<>());
}
