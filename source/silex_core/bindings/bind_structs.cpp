// (C) Copyright 2026 Floating Rock Studio Ltd
// SPDX-License-Identifier: MIT

/// @file bind_structs.cpp
/// @brief Python bindings for Silex data structures and Box accessor.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <silex/Box.h>
#include <silex/structs.h>
#include "bind_helpers.h"

namespace py = pybind11;

namespace {

/// Convert Box to Python dict (recursive).
py::dict boxToDict(const silex::Box& box) {
    py::dict result;
    for (const auto& [k, v] : box.data()) {
        if (auto* m = std::any_cast<silex::ContextMap>(&v)) {
            result[py::cast(k)] = boxToDict(silex::Box(*m));
        } else {
            result[py::cast(k)] = silex_bind::anyToPy(v);
        }
    }
    return result;
}

} // anonymous namespace

void bindStructs(py::module_& m) {

    // MARK: Box

    py::class_<silex::Box>(m, "Box", "Dictionary-like wrapper for nested resolver context data.")
        .def(py::init<>())
        .def(py::init([](const silex::Box& other) {
            return silex::Box(other.data());
        }), py::arg("data"))
        .def(py::init([](py::dict data) {
            silex::ContextMap map;
            for (auto item : data) {
                map[item.first.cast<std::string>()] = silex_bind::pyToAny(item.second);
            }
            return silex::Box(std::move(map));
        }), py::arg("data") = py::dict())
        .def(py::init([](py::kwargs kwargs) {
            silex::ContextMap map;
            for (auto item : kwargs) {
                map[item.first.cast<std::string>()] = silex_bind::pyToAny(item.second);
            }
            return silex::Box(std::move(map));
        }))
        .def("__getattr__", [](const silex::Box& self, const std::string& key) -> py::object {
            auto val = self.data().find(key);
            if (val == self.data().end()) {
                return py::cast(silex::Box());
            }
            return silex_bind::anyToPy(val->second);
        })
        .def("__getitem__", [](const silex::Box& self, const std::string& key) -> py::object {
            auto val = self.data().find(key);
            if (val == self.data().end()) {
                throw py::key_error(key);
            }
            return silex_bind::anyToPy(val->second);
        })
        .def("__setattr__", [](silex::Box& self, const std::string& key, py::object value) {
            self.data()[key] = silex_bind::pyToAny(value);
        })
        .def("__setitem__", [](silex::Box& self, const std::string& key, py::object value) {
            self.data()[key] = silex_bind::pyToAny(value);
        })
        .def("__delitem__", [](silex::Box& self, const std::string& key) {
            auto it = self.data().find(key);
            if (it == self.data().end()) throw py::key_error(key);
            self.data().erase(it);
        })
        .def("__contains__", [](const silex::Box& self, const std::string& key) {
            return self.has(key);
        })
        .def("__len__", [](const silex::Box& self) {
            return self.data().size();
        })
        .def("__bool__", [](const silex::Box& self) {
            return !self.data().empty();
        })
        .def("__iter__", [](const silex::Box& self) {
            return py::make_key_iterator(self.data().begin(), self.data().end());
        }, py::keep_alive<0, 1>())
        .def("get", [](const silex::Box& self, const std::string& key, py::object defaultVal) -> py::object {
            auto it = self.data().find(key);
            if (it == self.data().end()) return defaultVal;
            return silex_bind::anyToPy(it->second);
        }, py::arg("key"), py::arg("default") = py::none(), "Return a value by key with an optional default.")
        .def("keys", [](const silex::Box& self) {
            py::list result;
            for (const auto& [k, _] : self.data()) {
                result.append(py::cast(k));
            }
            return result;
        })
        .def("values", [](const silex::Box& self) {
            py::list result;
            for (const auto& [_, v] : self.data()) {
                result.append(silex_bind::anyToPy(v));
            }
            return result;
        })
        .def("items", [](const silex::Box& self) {
            py::list result;
            for (const auto& [k, v] : self.data()) {
                result.append(py::make_tuple(py::cast(k), silex_bind::anyToPy(v)));
            }
            return result;
        })
        .def("to_dict", [](const silex::Box& self) {
            return boxToDict(self);
        }, "Convert the box to a plain Python dictionary.")
        .def("__repr__", [](const silex::Box& self) {
            return "Box(" + py::repr(boxToDict(self)).cast<std::string>() + ")";
        })
        .def("__str__", [](const silex::Box& self) {
            return py::str(boxToDict(self)).cast<std::string>();
        })
        .def("__eq__", [](const silex::Box& self, py::object other) -> bool {
            if (py::isinstance<silex::Box>(other)) {
                return boxToDict(self).equal(boxToDict(other.cast<silex::Box>()));
            }
            if (py::isinstance<py::dict>(other)) {
                return boxToDict(self).equal(other);
            }
            return false;
        });

    // MARK: PlaceholderValue

    py::class_<silex::PlaceholderValue>(m, "PlaceholderValue", "Explicit placeholder marker used during path generation.")
        .def(py::init<const std::string&>(), py::arg("value") = "")
        .def("value", &silex::PlaceholderValue::value)
        .def("__str__", &silex::PlaceholderValue::str)
        .def("__repr__", [](const silex::PlaceholderValue& p) {
            return "PlaceholderValue('" + p.value() + "')";
        })
        .def("__format__", [](const silex::PlaceholderValue& p, const std::string&) {
            return p.value();
        });

    // MARK: SilexConfig

    py::class_<silex::SilexConfig>(m, "SilexConfig", "Resolver configuration values shared across schemas and functors.")
        .def(py::init<>())
        .def(py::init([](py::kwargs kwargs) {
            silex::SilexConfig cfg;
            if (kwargs.contains("global_variables"))
                cfg.globalVariables = silex_bind::toContextMap(kwargs["global_variables"]);
            if (kwargs.contains("functor_variables"))
                cfg.functorVariables = silex_bind::toContextMap(kwargs["functor_variables"]);
            if (kwargs.contains("placeholder_variables"))
                cfg.placeholderVariables = silex_bind::toContextMap(kwargs["placeholder_variables"]);
            if (kwargs.contains("case_sensitive"))
                cfg.caseSensitive = kwargs["case_sensitive"].cast<bool>();
            return cfg;
        }))
        .def_property("global_variables",
            [](const silex::SilexConfig& c) { return silex::Box(c.globalVariables); },
            [](silex::SilexConfig& c, py::object v) { c.globalVariables = silex_bind::toContextMap(v); })
        .def_property("functor_variables",
            [](const silex::SilexConfig& c) { return silex::Box(c.functorVariables); },
            [](silex::SilexConfig& c, py::object v) { c.functorVariables = silex_bind::toContextMap(v); })
        .def_property("placeholder_variables",
            [](const silex::SilexConfig& c) { return silex::Box(c.placeholderVariables); },
            [](silex::SilexConfig& c, py::object v) { c.placeholderVariables = silex_bind::toContextMap(v); })
        .def_readwrite("case_sensitive", &silex::SilexConfig::caseSensitive);

    // MARK: SilexParseOptions

    py::class_<silex::SilexParseOptions>(m, "SilexParseOptions", "Options that constrain schema loading and resolver traversal.")
        .def(py::init([](py::kwargs kwargs) {
            silex::SilexParseOptions opts;
            if (kwargs.contains("include_deprecated"))
                opts.includeDeprecated = kwargs["include_deprecated"].cast<bool>();
            if (kwargs.contains("allow_partial"))
                opts.allowPartial = kwargs["allow_partial"].cast<bool>();
            if (kwargs.contains("max_backtrack_iterations"))
                opts.maxBacktrackIterations = kwargs["max_backtrack_iterations"].cast<int>();
            if (kwargs.contains("verbosity"))
                opts.verbosity = kwargs["verbosity"].cast<int>();
            if (kwargs.contains("endpoint")) {
                auto ep = kwargs["endpoint"];
                if (!ep.is_none()) {
                    if (py::isinstance<py::str>(ep))
                        opts.endpoint = std::vector<std::string>{ep.cast<std::string>()};
                    else
                        opts.endpoint = ep.cast<std::vector<std::string>>();
                }
            }
            if (kwargs.contains("schema")) {
                auto sc = kwargs["schema"];
                if (!sc.is_none()) {
                    if (py::isinstance<py::str>(sc))
                        opts.schema = std::vector<std::string>{sc.cast<std::string>()};
                    else
                        opts.schema = sc.cast<std::vector<std::string>>();
                }
            }
            if (kwargs.contains("segment_limit")) {
                auto sl = kwargs["segment_limit"];
                if (!sl.is_none()) {
                    if (py::isinstance<py::tuple>(sl)) {
                        auto t = sl.cast<py::tuple>();
                        opts.segmentLimit = std::make_pair(t[0].cast<int>(), t[1].cast<int>());
                    } else {
                        opts.segmentLimit = sl.cast<int>();
                    }
                }
            }
            if (kwargs.contains("placeholders")) {
                auto pl = kwargs["placeholders"];
                if (!pl.is_none()) {
                    opts.placeholders = pl.cast<std::map<std::string, std::string>>();
                }
            }
            return opts;
        }))
        .def_readwrite("include_deprecated", &silex::SilexParseOptions::includeDeprecated)
        .def_readwrite("allow_partial", &silex::SilexParseOptions::allowPartial)
        .def_readwrite("max_backtrack_iterations", &silex::SilexParseOptions::maxBacktrackIterations)
        .def_readwrite("verbosity", &silex::SilexParseOptions::verbosity);

    // MARK: ResolvedValue

    py::class_<silex::ResolvedValue>(m, "ResolvedValue", "Resolved output value that may carry ambiguity information.")
        .def(py::init<>())
        .def(py::init([](py::object value, bool isAmbiguous) {
            silex::ResolvedValue rv;
            rv.value = silex_bind::pyToAny(value);
            rv.isAmbiguous = isAmbiguous;
            return rv;
        }), py::arg("value") = py::none(), py::arg("is_ambiguous") = false)
        .def_property("value",
            [](const silex::ResolvedValue& self) { return silex_bind::anyToPy(self.value); },
            [](silex::ResolvedValue& self, py::object val) { self.value = silex_bind::pyToAny(val); })
        .def_readwrite("is_ambiguous", &silex::ResolvedValue::isAmbiguous);

    // MARK: SilexSchemaInfo

    py::class_<silex::SilexSchemaInfo>(m, "SilexSchemaInfo", "Metadata describing a discovered `.silex` schema.")
        .def(py::init<>())
        .def_property("path",
            [](const silex::SilexSchemaInfo& s) { return silex_bind::optStrToPy(s.path); },
            [](silex::SilexSchemaInfo& s, py::object v) { s.path = silex_bind::pyToOptStr(v); })
        .def_readwrite("uid", &silex::SilexSchemaInfo::uid)
        .def_readwrite("root_path", &silex::SilexSchemaInfo::rootPath)
        .def_readwrite("path_pattern", &silex::SilexSchemaInfo::pathPattern)
        .def_property("context_filters",
            [](const silex::SilexSchemaInfo& s) {
                py::list result;
                for (const auto& filterMap : s.contextFilters) {
                    py::dict d;
                    for (const auto& [k, v] : filterMap) {
                        d[py::cast(k)] = silex_bind::anyToPy(v);
                    }
                    result.append(d);
                }
                return result;
            },
            [](silex::SilexSchemaInfo& s, py::list filters) {
                s.contextFilters.clear();
                for (auto item : filters) {
                    std::map<std::string, std::any> filterMap;
                    for (auto kv : item.cast<py::dict>()) {
                        filterMap[kv.first.cast<std::string>()] = silex_bind::pyToAny(kv.second);
                    }
                    s.contextFilters.push_back(std::move(filterMap));
                }
            })
        .def_readwrite("segmenter_uid", &silex::SilexSchemaInfo::segmenterUid)
        .def_readwrite("functor_uids", &silex::SilexSchemaInfo::functorUids)
        .def_readwrite("functor_aliases", &silex::SilexSchemaInfo::functorAliases)
        .def_readwrite("endpoints", &silex::SilexSchemaInfo::endpoints)
        .def_property("extends",
            [](const silex::SilexSchemaInfo& s) { return silex_bind::optStrToPy(s.extends); },
            [](silex::SilexSchemaInfo& s, py::object v) { s.extends = silex_bind::pyToOptStr(v); });

    // MARK: SilexResolveMatch

    py::class_<silex::SilexResolveMatch>(m, "SilexResolveMatch", "Single candidate match produced during path or context resolution.")
        .def(py::init<>())
        .def_property_readonly("status", [](const silex::SilexResolveMatch& s) {
            return s.status;
        })
        .def_property_readonly("source_path", [](const silex::SilexResolveMatch& s) {
            return silex_bind::optStrToPy(s.sourcePath);
        })
        .def_property_readonly("resolved_path", [](const silex::SilexResolveMatch& s) {
            return silex_bind::optStrToPy(s.resolvedPath);
        })
        .def_property_readonly("unresolved_path", [](const silex::SilexResolveMatch& s) {
            return silex_bind::optStrToPy(s.unresolvedPath);
        })
        .def_property_readonly("context", [](const silex::SilexResolveMatch& s) {
            return silex::Box(s.context);
        })
        .def_property_readonly("schema_uid", [](const silex::SilexResolveMatch& s) {
            return silex_bind::optStrToPy(s.schemaUid);
        })
        .def_property_readonly("schema_endpoint", [](const silex::SilexResolveMatch& s) {
            return silex_bind::optStrToPy(s.schemaEndpoint);
        })
        .def_property_readonly("schema_endpoint_path", [](const silex::SilexResolveMatch& s) {
            return silex_bind::optStrToPy(s.schemaEndpointPath);
        })
        .def_property_readonly("used_deprecated_traversal", [](const silex::SilexResolveMatch& s) {
            return s.usedDeprecatedTraversal;
        });

    // MARK: SilexContextResolve

    py::class_<silex::SilexContextResolve>(m, "SilexContextResolve", "Result object returned by `context_from_path`.")
        .def(py::init([](py::kwargs kwargs) {
            silex::SilexContextResolve r;
            if (kwargs.contains("status"))
                r.status = kwargs["status"].cast<silex::ResolverStatus>();
            if (kwargs.contains("source_path"))
                r.sourcePath = silex_bind::pyToOptStr(kwargs["source_path"]);
            if (kwargs.contains("resolved_path"))
                r.resolvedPath = silex_bind::pyToOptStr(kwargs["resolved_path"]);
            if (kwargs.contains("unresolved_path"))
                r.unresolvedPath = silex_bind::pyToOptStr(kwargs["unresolved_path"]);
            if (kwargs.contains("context")) {
                auto ctx = kwargs["context"];
                if (!ctx.is_none()) r.context = silex_bind::toContextMap(ctx);
            }
            if (kwargs.contains("schema_uid"))
                r.schemaUid = silex_bind::pyToOptStr(kwargs["schema_uid"]);
            if (kwargs.contains("schema_endpoint"))
                r.schemaEndpoint = silex_bind::pyToOptStr(kwargs["schema_endpoint"]);
            if (kwargs.contains("schema_endpoint_path"))
                r.schemaEndpointPath = silex_bind::pyToOptStr(kwargs["schema_endpoint_path"]);
            if (kwargs.contains("used_deprecated_traversal"))
                r.usedDeprecatedTraversal = kwargs["used_deprecated_traversal"].cast<bool>();
            return r;
        }))
        .def_property_readonly("status", [](const silex::SilexContextResolve& s) {
            return s.status;
        })
        .def_property_readonly("source_path", [](const silex::SilexContextResolve& s) {
            return silex_bind::optStrToPy(s.sourcePath);
        })
        .def_property_readonly("resolved_path", [](const silex::SilexContextResolve& s) {
            return silex_bind::optStrToPy(s.resolvedPath);
        })
        .def_property_readonly("unresolved_path", [](const silex::SilexContextResolve& s) {
            return silex_bind::optStrToPy(s.unresolvedPath);
        })
        .def_property_readonly("context", [](const silex::SilexContextResolve& s) {
            return silex::Box(s.context);
        })
        .def_property_readonly("schema_uid", [](const silex::SilexContextResolve& s) {
            return silex_bind::optStrToPy(s.schemaUid);
        })
        .def_property_readonly("schema_endpoint", [](const silex::SilexContextResolve& s) {
            return silex_bind::optStrToPy(s.schemaEndpoint);
        })
        .def_property_readonly("schema_endpoint_path", [](const silex::SilexContextResolve& s) {
            return silex_bind::optStrToPy(s.schemaEndpointPath);
        })
        .def_property_readonly("used_deprecated_traversal", [](const silex::SilexContextResolve& s) {
            return s.usedDeprecatedTraversal;
        })
        .def_property_readonly("matches", [](const silex::SilexContextResolve& s) {
            if (!s.matches.empty()) return s.matches;
            // Synthesize a match from the result fields if none were populated.
            if (s.status == silex::ResolverStatus::Error) return s.matches;
            if (!s.resolvedPath && !s.schemaUid && !s.schemaEndpoint) return s.matches;
            silex::SilexResolveMatch m;
            m.status = s.status;
            m.sourcePath = s.sourcePath;
            m.resolvedPath = s.resolvedPath;
            m.unresolvedPath = s.unresolvedPath;
            m.context = s.context;
            m.schemaUid = s.schemaUid;
            m.schemaEndpoint = s.schemaEndpoint;
            m.schemaEndpointPath = s.schemaEndpointPath;
            m.usedDeprecatedTraversal = s.usedDeprecatedTraversal;
            return std::vector<silex::SilexResolveMatch>{m};
        });

    // MARK: SilexPathResolve

    py::class_<silex::SilexPathResolve>(m, "SilexPathResolve", "Result object returned by `path_from_context`.")
        .def(py::init([](py::kwargs kwargs) {
            silex::SilexPathResolve r;
            if (kwargs.contains("status"))
                r.status = kwargs["status"].cast<silex::ResolverStatus>();
            if (kwargs.contains("resolved_path"))
                r.resolvedPath = silex_bind::pyToOptStr(kwargs["resolved_path"]);
            if (kwargs.contains("context")) {
                auto ctx = kwargs["context"];
                if (!ctx.is_none()) r.context = silex_bind::toContextMap(ctx);
            }
            if (kwargs.contains("missing_context"))
                r.missingContext = kwargs["missing_context"].cast<std::vector<std::string>>();
            if (kwargs.contains("schema_uid"))
                r.schemaUid = silex_bind::pyToOptStr(kwargs["schema_uid"]);
            if (kwargs.contains("schema_endpoint"))
                r.schemaEndpoint = silex_bind::pyToOptStr(kwargs["schema_endpoint"]);
            if (kwargs.contains("schema_endpoint_path"))
                r.schemaEndpointPath = silex_bind::pyToOptStr(kwargs["schema_endpoint_path"]);
            if (kwargs.contains("used_deprecated_traversal"))
                r.usedDeprecatedTraversal = kwargs["used_deprecated_traversal"].cast<bool>();
            if (kwargs.contains("furthest_segment"))
                r.furthestSegment = silex_bind::pyToOptStr(kwargs["furthest_segment"]);
            return r;
        }))
        .def_property_readonly("status", [](const silex::SilexPathResolve& s) {
            return s.status;
        })
        .def_property_readonly("resolved_path", [](const silex::SilexPathResolve& s) {
            return silex_bind::optStrToPy(s.resolvedPath);
        })
        .def_property_readonly("context", [](const silex::SilexPathResolve& s) {
            return silex::Box(s.context);
        })
        .def_property_readonly("missing_context", [](const silex::SilexPathResolve& s) {
            return s.missingContext;
        })
        .def_property_readonly("schema_uid", [](const silex::SilexPathResolve& s) {
            return silex_bind::optStrToPy(s.schemaUid);
        })
        .def_property_readonly("schema_endpoint", [](const silex::SilexPathResolve& s) {
            return silex_bind::optStrToPy(s.schemaEndpoint);
        })
        .def_property_readonly("schema_endpoint_path", [](const silex::SilexPathResolve& s) {
            return silex_bind::optStrToPy(s.schemaEndpointPath);
        })
        .def_property_readonly("used_deprecated_traversal", [](const silex::SilexPathResolve& s) {
            return s.usedDeprecatedTraversal;
        })
        .def_property_readonly("furthest_segment", [](const silex::SilexPathResolve& s) {
            return silex_bind::optStrToPy(s.furthestSegment);
        })
        .def_property_readonly("matches", [](const silex::SilexPathResolve& s) {
            if (!s.matches.empty()) return s.matches;
            if (s.status == silex::ResolverStatus::Error) return s.matches;
            if (!s.resolvedPath && !s.schemaUid && !s.schemaEndpoint) return s.matches;
            silex::SilexResolveMatch m;
            m.status = s.status;
            m.resolvedPath = s.resolvedPath;
            m.context = s.context;
            m.schemaUid = s.schemaUid;
            m.schemaEndpoint = s.schemaEndpoint;
            m.schemaEndpointPath = s.schemaEndpointPath;
            m.usedDeprecatedTraversal = s.usedDeprecatedTraversal;
            return std::vector<silex::SilexResolveMatch>{m};
        });

    // MARK: FunctorOutput

    py::class_<silex::FunctorOutput>(m, "FunctorOutput", "Named functor output channel and its valid option set.")
        .def(py::init<>())
        .def(py::init([](const std::string& name, std::vector<std::string> options) {
            silex::FunctorOutput fo;
            fo.name = name;
            fo.options = std::move(options);
            return fo;
        }), py::arg("name"), py::arg("options") = std::vector<std::string>{})
        .def_readwrite("name", &silex::FunctorOutput::name)
        .def_readwrite("options", &silex::FunctorOutput::options)
        .def("__repr__", [](const silex::FunctorOutput& fo) {
            return "FunctorOutput(name='" + fo.name + "')";
        });

    // MARK: FunctorContext

    py::class_<silex::FunctorContext>(m, "FunctorContext", "Context passed to functor parse and format operations.")
        .def(py::init<>())
        .def(py::init([](py::object context, const std::string& parent,
                         const std::string& segment, py::object variables) {
            silex::FunctorContext fc;
            if (!context.is_none()) fc.context = silex_bind::toContextMap(context);
            fc.parent = parent;
            fc.segment = segment;
            if (!variables.is_none()) fc.variables = silex_bind::toContextMap(variables);
            return fc;
        }),
            py::arg("context") = py::none(),
            py::arg("parent") = "",
            py::arg("segment") = "",
            py::arg("variables") = py::none())
        .def_property("context",
            [](const silex::FunctorContext& fc) { return silex::Box(fc.context); },
            [](silex::FunctorContext& fc, py::object ctx) { fc.context = silex_bind::toContextMap(ctx); })
        .def_readwrite("parent", &silex::FunctorContext::parent)
        .def_readwrite("segment", &silex::FunctorContext::segment)
        .def_property("variables",
            [](const silex::FunctorContext& fc) { return silex::Box(fc.variables); },
            [](silex::FunctorContext& fc, py::object vars) { fc.variables = silex_bind::toContextMap(vars); });

    // MARK: ParseResult

    py::class_<silex::ParseResult>(m, "ParseResult", "Output of a functor parse operation.")
        .def(py::init<>())
        .def(py::init([](py::kwargs kwargs) {
            silex::ParseResult r;
            if (kwargs.contains("success")) r.success = kwargs["success"].cast<bool>();
            if (kwargs.contains("message")) r.message = kwargs["message"].cast<std::string>();
            if (kwargs.contains("outputs")) {
                auto outputs = kwargs["outputs"].cast<py::dict>();
                for (auto item : outputs) {
                    r.outputs[item.first.cast<std::string>()] = item.second.cast<silex::ResolvedValue>();
                }
            }
            return r;
        }))
        .def_readwrite("success", &silex::ParseResult::success)
        .def_readwrite("message", &silex::ParseResult::message)
        .def_property_readonly("outputs", [](const silex::ParseResult& rr) {
            py::dict result;
            for (const auto& [k, v] : rr.outputs) {
                result[py::cast(k)] = py::cast(v);
            }
            return result;
        });

    // MARK: FormatResult

    py::class_<silex::FormatResult>(m, "FormatResult", "Output of a functor format operation.")
        .def(py::init<>())
        .def(py::init([](py::kwargs kwargs) {
            silex::FormatResult r;
            if (kwargs.contains("success")) r.success = kwargs["success"].cast<bool>();
            if (kwargs.contains("message")) r.message = kwargs["message"].cast<std::string>();
            if (kwargs.contains("output")) r.output = kwargs["output"].cast<std::string>();
            if (kwargs.contains("matches")) r.matches = kwargs["matches"].cast<std::vector<std::string>>();
            return r;
        }))
        .def_readwrite("success", &silex::FormatResult::success)
        .def_readwrite("message", &silex::FormatResult::message)
        .def_readwrite("output", &silex::FormatResult::output)
        .def_readwrite("matches", &silex::FormatResult::matches);

    // MARK: ExternalResource

    py::class_<silex::ExternalResource>(m, "ExternalResource")
        .def(py::init<>())
        .def(py::init([](py::kwargs kwargs) {
            silex::ExternalResource r;
            if (kwargs.contains("uid")) r.uid = kwargs["uid"].cast<std::string>();
            if (kwargs.contains("name")) r.name = kwargs["name"].cast<std::string>();
            if (kwargs.contains("module")) r.module = kwargs["module"].cast<std::string>();
            if (kwargs.contains("language")) r.language = kwargs["language"].cast<silex::Language>();
            if (kwargs.contains("package")) r.package = kwargs["package"].cast<std::string>();
            return r;
        }))
        .def_readwrite("uid", &silex::ExternalResource::uid)
        .def_readwrite("name", &silex::ExternalResource::name)
        .def_readwrite("module", &silex::ExternalResource::module)
        .def_readwrite("language", &silex::ExternalResource::language)
        .def_readwrite("package", &silex::ExternalResource::package);

    // MARK: SilexFunctorInfo

    py::class_<silex::SilexFunctorInfo, silex::ExternalResource>(m, "SilexFunctorInfo")
        .def(py::init<>())
        .def(py::init([](py::kwargs kwargs) {
            silex::SilexFunctorInfo r;
            if (kwargs.contains("uid")) r.uid = kwargs["uid"].cast<std::string>();
            if (kwargs.contains("name")) r.name = kwargs["name"].cast<std::string>();
            if (kwargs.contains("module")) r.module = kwargs["module"].cast<std::string>();
            if (kwargs.contains("language")) r.language = kwargs["language"].cast<silex::Language>();
            if (kwargs.contains("package")) r.package = kwargs["package"].cast<std::string>();
            if (kwargs.contains("aliases")) r.aliases = kwargs["aliases"].cast<std::vector<std::string>>();
            return r;
        }))
        .def_readwrite("aliases", &silex::SilexFunctorInfo::aliases);

    // MARK: ExpressionInput

    py::class_<silex::ExpressionInput>(m, "ExpressionInput")
        .def(py::init<>())
        .def(py::init([](py::kwargs kwargs) {
            silex::ExpressionInput ei;
            if (kwargs.contains("raw")) ei.raw = kwargs["raw"].cast<std::string>();
            if (kwargs.contains("value")) ei.value = silex_bind::pyToFunctorInput(kwargs["value"]);
            if (kwargs.contains("is_dynamic")) ei.isDynamic = kwargs["is_dynamic"].cast<bool>();
            return ei;
        }))
        .def_readwrite("raw", &silex::ExpressionInput::raw)
        .def_property("value",
            [](const silex::ExpressionInput& ei) { return silex_bind::functorInputToPy(ei.value); },
            [](silex::ExpressionInput& ei, py::object v) { ei.value = silex_bind::pyToFunctorInput(v); })
        .def_readwrite("is_dynamic", &silex::ExpressionInput::isDynamic);

    // MARK: SilexExpression

    py::class_<silex::SilexExpression>(m, "SilexExpression")
        .def(py::init<>())
        .def(py::init([](const std::string& raw,
                         std::vector<silex::ExpressionInput> inputs,
                         std::vector<std::string> outputs,
                         py::object functor_info,
                         std::vector<silex::FunctorOutput> functor_outputs,
                         silex::ExpressionMode mode) {
            silex::SilexExpression se;
            se.raw = raw;
            se.inputs = std::move(inputs);
            se.outputs = std::move(outputs);
            if (!functor_info.is_none())
                se.functorInfo = functor_info.cast<silex::SilexFunctorInfo>();
            se.functorOutputs = std::move(functor_outputs);
            se.mode = mode;
            return se;
        }),
            py::arg("raw") = "",
            py::arg("inputs") = std::vector<silex::ExpressionInput>{},
            py::arg("outputs") = std::vector<std::string>{},
            py::arg("functor_info") = py::none(),
            py::arg("functor_outputs") = std::vector<silex::FunctorOutput>{},
            py::arg("mode") = silex::ExpressionMode::Parse)
        .def_readwrite("raw", &silex::SilexExpression::raw)
        .def_readwrite("functor_name", &silex::SilexExpression::functorName)
        .def_readwrite("inputs", &silex::SilexExpression::inputs)
        .def_readwrite("outputs", &silex::SilexExpression::outputs)
        .def_property("functor_info",
            [](const silex::SilexExpression& se) -> py::object {
                if (se.functorInfo) return py::cast(*se.functorInfo);
                return py::none();
            },
            [](silex::SilexExpression& se, py::object info) {
                if (info.is_none()) se.functorInfo = std::nullopt;
                else se.functorInfo = info.cast<silex::SilexFunctorInfo>();
            })
        .def_readwrite("functor_outputs", &silex::SilexExpression::functorOutputs)
        .def_readwrite("mode", &silex::SilexExpression::mode)
        .def_readwrite("warnings", &silex::SilexExpression::warnings);

    // MARK: SilexExpressionGraph

    py::class_<silex::SilexExpressionGraph>(m, "SilexExpressionGraph")
        .def(py::init<>())
        .def(py::init([](std::vector<silex::SilexExpression> expressions,
                         std::vector<std::string> inputs,
                         std::vector<std::string> outputs,
                         std::map<std::pair<int,int>, std::pair<int,int>> connections,
                         silex::ExpressionMode mode) {
            silex::SilexExpressionGraph g;
            g.expressions = std::move(expressions);
            g.inputs = std::move(inputs);
            g.outputs = std::move(outputs);
            g.connections = std::move(connections);
            g.mode = mode;
            return g;
        }),
            py::arg("expressions") = std::vector<silex::SilexExpression>{},
            py::arg("inputs") = std::vector<std::string>{},
            py::arg("outputs") = std::vector<std::string>{},
            py::arg("connections") = std::map<std::pair<int,int>, std::pair<int,int>>{},
            py::arg("mode") = silex::ExpressionMode::Parse)
        .def_readwrite("expressions", &silex::SilexExpressionGraph::expressions)
        .def_readwrite("inputs", &silex::SilexExpressionGraph::inputs)
        .def_readwrite("outputs", &silex::SilexExpressionGraph::outputs)
        .def_readwrite("connections", &silex::SilexExpressionGraph::connections)
        .def_readwrite("mode", &silex::SilexExpressionGraph::mode);

    // MARK: Target

    py::class_<silex::Target>(m, "Target")
        .def(py::init<>())
        .def_property("group",
            [](const silex::Target& t) -> py::object {
                if (!t.group) return py::none();
                return std::visit([](auto&& v) -> py::object { return py::cast(v); }, *t.group);
            },
            [](silex::Target& t, py::object v) {
                if (v.is_none()) t.group = std::nullopt;
                else if (py::isinstance<py::int_>(v)) t.group = v.cast<int>();
                else t.group = v.cast<std::string>();
            })
        .def_property("variable",
            [](const silex::Target& t) { return silex_bind::optStrToPy(t.variable); },
            [](silex::Target& t, py::object v) { t.variable = silex_bind::pyToOptStr(v); })
        .def_readwrite("type", &silex::Target::type)
        .def_readwrite("is_array", &silex::Target::isArray);

    // MARK: SilexSegment

    py::class_<silex::SilexSegment, std::shared_ptr<silex::SilexSegment>>(m, "SilexSegment")
        .def(py::init<>())
        .def_readwrite("name", &silex::SilexSegment::name)
        .def_property("pattern",
            [](const silex::SilexSegment& s) { return silex_bind::optStrToPy(s.pattern); },
            [](silex::SilexSegment& s, py::object v) { s.pattern = silex_bind::pyToOptStr(v); })
        .def_readwrite("flags", &silex::SilexSegment::flags)
        .def_property("parse",
            [](const silex::SilexSegment& s) {
                py::list result;
                for (const auto& m : s.parse) {
                    py::dict d;
                    for (const auto& [k, v] : m) d[py::cast(k)] = silex_bind::anyToPy(v);
                    result.append(d);
                }
                return result;
            },
            [](silex::SilexSegment& s, py::list val) {
                s.parse.clear();
                for (auto item : val) {
                    std::map<std::string, std::any> m;
                    for (auto kv : item.cast<py::dict>())
                        m[kv.first.cast<std::string>()] = silex_bind::pyToAny(kv.second);
                    s.parse.push_back(std::move(m));
                }
            })
        .def_property("format",
            [](const silex::SilexSegment& s) {
                py::list result;
                for (const auto& m : s.format) {
                    py::dict d;
                    for (const auto& [k, v] : m) d[py::cast(k)] = silex_bind::anyToPy(v);
                    result.append(d);
                }
                return result;
            },
            [](silex::SilexSegment& s, py::list val) {
                s.format.clear();
                for (auto item : val) {
                    std::map<std::string, std::any> m;
                    for (auto kv : item.cast<py::dict>())
                        m[kv.first.cast<std::string>()] = silex_bind::pyToAny(kv.second);
                    s.format.push_back(std::move(m));
                }
            })
        .def_readwrite("format_update_keys", &silex::SilexSegment::formatUpdateKeys)
        .def_readwrite("targets", &silex::SilexSegment::targets)
        .def_property("template",
            [](const silex::SilexSegment& s) { return s.templateName; },
            [](silex::SilexSegment& s, const std::string& v) { s.templateName = v; })
        .def_property("parent",
            [](const silex::SilexSegment& s) -> py::object {
                auto p = s.parent.lock();
                if (p) return py::cast(p);
                return py::none();
            },
            [](silex::SilexSegment& s, py::object v) {
                if (v.is_none()) s.parent.reset();
                else s.parent = v.cast<std::shared_ptr<silex::SilexSegment>>();
            })
        .def_readwrite("placeholders", &silex::SilexSegment::placeholders)
        .def_readwrite("branches", &silex::SilexSegment::branches)
        .def_readwrite("endpoint", &silex::SilexSegment::endpoint)
        .def_property("partition_segmenter",
            [](const silex::SilexSegment& s) { return silex_bind::optStrToPy(s.partitionSegmenter); },
            [](silex::SilexSegment& s, py::object v) { s.partitionSegmenter = silex_bind::pyToOptStr(v); })
        .def_readwrite("partitions", &silex::SilexSegment::partitions)
        .def_readwrite("ordered_partitions", &silex::SilexSegment::orderedPartitions)
        .def_readwrite("branch_endpoints", &silex::SilexSegment::branchEndpoints);

    // MARK: SilexSchema

    py::class_<silex::SilexSchema, std::shared_ptr<silex::SilexSchema>>(m, "SilexSchema")
        .def(py::init<>())
        .def_readwrite("info", &silex::SilexSchema::info)
        .def_readwrite("root_segments", &silex::SilexSchema::rootSegments)
        .def_readwrite("config", &silex::SilexSchema::config);
}
