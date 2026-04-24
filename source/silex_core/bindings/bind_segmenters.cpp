/// @file bind_segmenters.cpp
/// @brief Python bindings for segmenter implementations.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <silex/interfaces/ISegmenter.h>
#include "segmenters/FilesystemSegmenter.h"
#include "segmenters/URISegmenter.h"

namespace py = pybind11;
using namespace silex;

void bindSegmenters(py::module_& m) {
    py::class_<segmenters::FilesystemSegmenter, ISegmenter, std::shared_ptr<segmenters::FilesystemSegmenter>>(m, "FilesystemSegmenter")
        .def(py::init<>());

    py::class_<segmenters::URISegmenter, ISegmenter, std::shared_ptr<segmenters::URISegmenter>>(m, "URISegmenter")
        .def(py::init<>());
}
