# Silex API Guide

Silex is a path resolution library that maps filesystem paths to structured context dictionaries and back. It uses `.silex` schema files to define path patterns, segments, and templates.

The implementation is a C++17 core library with pybind11 bindings. Python code imports the compiled `silex` module, while C++ code uses the headers under `include/silex` and the `silex_core` shared library.

This guide is usage-oriented. The packaged build also generates a Doxygen HTML reference site under `docs/html/` for the public API and accompanying documentation sources.

- **Python module:** `silex`
- **Primary C++ resolver type:** `silex::resolvers::GenericResolver`
- **Shared C++ types namespace:** `silex`

For schema authoring, see `schema_authoring.md`.

---

## Quick Start

### Python

```python
import os
os.environ["SILEX_SCHEMA_PATH"] = "/path/to/schemas"

from silex import GenericResolver, ResolverStatus, set_verbosity, Verbosity

set_verbosity(Verbosity.Info)
resolver = GenericResolver()

# Path → Context  (returns a pybind-exposed SilexContextResolve object)
result = resolver.context_from_path("/projects/PROJ/assets/chrBob/model/v001/scene.ma")
print(result.status)              # ResolverStatus.SUCCESS
print(result.context.project)     # "PROJ"  (Box attribute access)
print(result.context.entity)      # "chrBob"

# Context → Path  (returns a pybind-exposed SilexPathResolve object)
result = resolver.path_from_context({
    "project": "PROJ",
    "entity": "chrBob",
    "fragment": "model",
    "version": 1,
    "component": "scene",
    "ext": "ma"
}, endpoint=["component"])
print(result.resolved_path)
```

### C++

```cpp
#include <silex/Box.h>
#include <silex/GenericResolver.h>
#include <silex/constants.h>

silex::setVerbosity(silex::Verbosity::Info);
silex::resolvers::GenericResolver resolver;

// Path → Context
auto result = resolver.contextFromPath("/projects/PROJ/assets/chrBob/model/v001/scene.ma");
// result.status == ResolverStatus::Success
silex::Box ctx(result.context);
auto project = ctx.get<std::string>("project");      // "PROJ"
auto entity  = ctx.get<std::string>("entity", "UNK"); // "chrBob", default "UNK"

// Context → Path
silex::Box writeCtx;
writeCtx.set("project",   std::string("PROJ"));
writeCtx.set("entity",    std::string("chrBob"));
writeCtx.set("fragment",  std::string("model"));
writeCtx.set("version",   1);
writeCtx.set("component", std::string("scene"));
writeCtx.set("ext",       std::string("ma"));

auto pathResult = resolver.pathFromContext(writeCtx.data(),
    std::vector<std::string>{"component"});
// pathResult.resolvedPath contains the generated path
```

---

## Installation & Setup

### Install With Pip

```bash
pip install fr-silex
```

The PyPI distribution name is `fr-silex`, while the import name stays `silex`.

### Build From Source

```bash
python -m pip install --upgrade pip build
python -m build
```

The CMake build installs the Python extension into `python/`, the public headers into `include/`, the shared library into `bin/` and `lib/`, and copies runtime DLLs needed for Python imports into `python/` during packaging.

### Environment

`SILEX_SCHEMA_PATH` must point to one or more directories containing `.silex` schema files. Multiple paths are separated by the platform path separator (`;` on Windows, `:` on Linux/macOS).

### Python Import

```python
from silex import GenericResolver
```

The root module also exposes enums and data types such as `ResolverStatus`, `Verbosity`, `Box`, `SilexParseOptions`, `SilexContextResolve`, and `SilexPathResolve`.

### C++ Include & Link

```cpp
#include <silex/GenericResolver.h>
```

Instantiate `silex::resolvers::GenericResolver` and link against `silex_core`.

---

## Creating a Resolver

### Python

```python
from silex import GenericResolver

resolver = GenericResolver(
    include_deprecated=False,   # Include deprecated segments
    schema="my-schema",         # Filter to specific schema
    config_id="default",        # Configuration ID
    segment_limit=(0, 5)        # Limit segment resolution range
)
```

All parameters are optional. A bare `GenericResolver()` loads all non-deprecated schemas found on `SILEX_SCHEMA_PATH`.

Python keyword arguments are translated to `SilexParseOptions` fields where applicable. For more explicit configuration, you can also pass a `SilexParseOptions` instance through the `options` argument.

### C++

```cpp
#include <silex/GenericResolver.h>

silex::SilexParseOptions options;
options.includeDeprecated = false;
// Other options set on SilexParseOptions struct

silex::resolvers::GenericResolver resolver(options, "my-schema", "default");
```

---

## API Reference

### context_from_path / contextFromPath

Resolves a filesystem path or URI to a context dictionary.

**Parameters:**

| Parameter | Python | C++ | Description |
|-----------|--------|-----|-------------|
| path | `str` | `const std::string&` | Filesystem path or URI to resolve |
| endpoint | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Target endpoint(s) to resolve to |
| schema | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Filter to specific schema(s) |

**Returns:** Python receives a `SilexContextResolve` object with snake_case properties. C++ receives a `silex::SilexContextResolve` struct. Key fields include `status`, `source_path`, `resolved_path`, `unresolved_path`, `context`, `schema_uid`, `schema_endpoint`, `schema_endpoint_path`, `used_deprecated_traversal`, and `matches`.

#### Python

```python
result = resolver.context_from_path(
    "/projects/PROJ/assets/chrBob/model/v001/scene.ma",
    endpoint="component",
    schema="my-schema"
)
print(result.status)
print(result.context)
print(result.schema_uid)
print(result.resolved_path)
print(result.unresolved_path)
```

#### C++

```cpp
auto result = resolver.contextFromPath(
    "/projects/PROJ/assets/chrBob/model/v001/scene.ma",
    {"component"},   // endpoint
    {"my-schema"}    // schema
);
// result.status, result.context, result.schemaUid, etc.
```

---

### path_from_context / pathFromContext

Generates a filesystem path from context values.

**Parameters:**

| Parameter | Python | C++ | Description |
|-----------|--------|-----|-------------|
| context | `dict` | `ContextMap` | Key-value pairs defining the context |
| endpoint | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Target endpoint(s) |
| schema | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Filter to specific schema(s) |
| include_children | `bool` (Python only, optional) | n/a | When `False`, Python returns the requested endpoint path itself. When `True`, descendant matches for that endpoint may be returned instead. |

**Returns:** Python receives a `SilexPathResolve` object with snake_case properties. C++ receives a `silex::SilexPathResolve` struct. Key fields include `status`, `resolved_path`, `context`, `missing_context`, `schema_uid`, `schema_endpoint`, `schema_endpoint_path`, `used_deprecated_traversal`, `furthest_segment`, and `matches`.

#### Python

```python
result = resolver.path_from_context({
    "project": "PROJ",
    "entity": "chrBob",
    "fragment": "model",
    "version": 1,
    "component": "scene",
    "ext": "ma"
}, endpoint=["component"])

print(result.resolved_path)
print(result.missing_context)  # Keys that were needed but not provided
```

When a requested endpoint also has child endpoints, Python defaults to returning the
requested endpoint path itself:

```python
parent = resolver.path_from_context(context, endpoint="take.dir")
child = resolver.path_from_context(context, endpoint="take.dir", include_children=True)

print(parent.resolved_path)  # .../v003.t02
print(child.resolved_path)   # .../v003.t02/example_v003.t02.metadata
```

#### C++

```cpp
silex::ContextMap context;
context["project"] = std::any(std::string("PROJ"));
context["entity"] = std::any(std::string("chrBob"));
context["fragment"] = std::any(std::string("model"));
context["version"] = std::any(1);
context["component"] = std::any(std::string("scene"));
context["ext"] = std::any(std::string("ma"));

auto result = resolver.pathFromContext(context,
    std::vector<std::string>{"component"});
// result.resolvedPath, result.missingContext, etc.
```

---

### available_templates / availableTemplates

Lists named templates from loaded schemas.

**Parameters:**

| Parameter | Python | C++ | Description |
|-----------|--------|-----|-------------|
| schema | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Filter to specific schema(s) |

**Returns:** List of template name strings.

#### Python

```python
templates = resolver.available_templates()
# ["version_tag", "entity_name", ...]

templates = resolver.available_templates(schema="my-schema")
```

#### C++

```cpp
auto templates = resolver.availableTemplates();
// std::vector<std::string>

auto templates = resolver.availableTemplates({"my-schema"});
```

---

### parse_template_value / parseTemplateValue

Extracts context from a string value using a named template's parse expressions.

**Parameters:**

| Parameter | Python | C++ | Description |
|-----------|--------|-----|-------------|
| value | `str` | `const std::string&` | The string value to parse |
| template_name | `str` | `const std::string&` | Name of the template to use |
| context | `dict` (optional) | `ContextMap` (optional) | Existing context to merge with |
| schema | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Filter to specific schema(s) |

**Returns:** Context dictionary/`ContextMap` with extracted values.

#### Python

```python
ctx = resolver.parse_template_value("v003", "version_tag")
# {"version": 3}

ctx = resolver.parse_template_value("chrBob_model", "asset_component",
                                     context={"project": "PROJ"})
```

#### C++

```cpp
auto ctx = resolver.parseTemplateValue("v003", "version_tag");
// ctx["version"] contains the extracted version

silex::ContextMap existing;
existing["project"] = std::any(std::string("PROJ"));
auto ctx = resolver.parseTemplateValue("chrBob_model", "asset_component", existing);
```

---

### format_template_value / formatTemplateValue

Generates a string value from context using a named template's format expression.

**Parameters:**

| Parameter | Python | C++ | Description |
|-----------|--------|-----|-------------|
| context | `dict` | `ContextMap` | Context with values to format |
| template_name | `str` | `const std::string&` | Name of the template to use |
| schema | `str \| list[str]` (optional) | `std::vector<std::string>` (optional) | Filter to specific schema(s) |

**Returns:** Formatted string.

#### Python

```python
value = resolver.format_template_value({"version": 3}, "version_tag")
# "v003"
```

#### C++

```cpp
silex::ContextMap context;
context["version"] = std::any(3);
auto value = resolver.formatTemplateValue(context, "version_tag");
// "v003"
```

---

### schema_from_path / schemaFromPath

Gets the schema UID that matches a given path.

**Parameters:**

| Parameter | Python | C++ | Description |
|-----------|--------|-----|-------------|
| path | `str` | `const std::string&` | Filesystem path to match |

**Returns:** Schema UID string, or `None` / `std::optional<std::string>` if no schema matches.

#### Python

```python
schema_uid = resolver.schema_from_path("/projects/PROJ/assets/chrBob/model/v001/scene.ma")
if schema_uid:
    print(f"Matched schema: {schema_uid}")
```

#### C++

```cpp
auto schemaUid = resolver.schemaFromPath("/projects/PROJ/assets/chrBob/model/v001/scene.ma");
if (schemaUid.has_value()) {
    std::cout << "Matched schema: " << schemaUid.value() << std::endl;
}
```

---

## Verbosity & Logging

### Python

```python
from silex import set_verbosity, Verbosity

set_verbosity(Verbosity.Quiet)    # No output
set_verbosity(Verbosity.Info)     # Basic info
set_verbosity(Verbosity.Flow)     # Resolution flow
set_verbosity(Verbosity.Detail)   # Detailed steps
set_verbosity(Verbosity.Trace)    # Full trace
```

The public Python API exposes `set_verbosity`. Lower-level binding controls such as `set_log_level` remain internal implementation details and should not be imported directly.

### C++

```cpp
#include <silex/constants.h>

silex::setVerbosity(silex::Verbosity::Quiet);
silex::setVerbosity(silex::Verbosity::Info);
silex::setVerbosity(silex::Verbosity::Flow);
silex::setVerbosity(silex::Verbosity::Detail);
silex::setVerbosity(silex::Verbosity::Trace);
```

---

## Enums Reference

### ResolverStatus

Bitflag enum — values can be combined.

| Value (C++) | Value (Python) | Description |
|-------------|----------------|-------------|
| `None` | `NONE` | No status |
| `Success` | `SUCCESS` | Full resolution succeeded |
| `MissingTargets` | `MISSING_TARGETS` | Required targets not found |
| `Partial` | `PARTIAL` | Partial match only |
| `Ambiguous` | `AMBIGUOUS` | Multiple matches found |
| `Error` | `ERROR` | Resolution error |

```python
from silex import ResolverStatus

if result.status == ResolverStatus.SUCCESS:
    ...
```

```cpp
if (silex::hasFlag(result.status, silex::ResolverStatus::Success)) {
    ...
}
```

### SegmentFlags

Bitflag enum for segment metadata.

| Value (C++) | Value (Python) | Description |
|-------------|----------------|-------------|
| `None` | `NONE` | No flags |
| `Deprecated` | `DEPRECATED` | Segment is deprecated |
| `ReadOnly` | `READONLY` | Segment is read-only |
| `Promote` | `PROMOTE` | Promote segment results |
| `Omit` | `OMIT` | Omit from path output |
| `Intermediate` | `INTERMEDIATE` | Intermediate segment |
| `HasPartitions` | `HAS_PARTITIONS` | Segment has partitions |
| `Fallback` | `FALLBACK` | Fallback segment |

### ExpressionMode

| Value | Description |
|-------|-------------|
| `Parse` | Parse a value into context |
| `Format` | Generate a value from context |

### Language

| Value | Description |
|-------|-------------|
| `Python` | Python expression |
| `JavaScript` | JavaScript expression |
| `Other` | Other expression language |

### Verbosity

| Value | Level | Description |
|-------|-------|-------------|
| `Quiet` | 0 | No output |
| `Info` | 1 | Basic information |
| `Flow` | 2 | Resolution flow |
| `Detail` | 3 | Detailed steps |
| `Trace` | 4 | Full trace |

---

## Data Structures Reference

| Struct | Key Fields | Description |
|--------|-----------|-------------|
| `PlaceholderValue` | `value` | Wildcard placeholder for glob pattern generation |
| `SilexConfig` | `globalVariables`, `functorVariables`, `placeholderVariables`, `caseSensitive` | Resolver configuration |
| `SilexParseOptions` | `endpoint`, `schema`, `segmentLimit`, `placeholders`, `allowPartial`, `includeDeprecated`, `maxBacktrackIterations` | Constructor/resolution options |
| `SilexSchemaInfo` | `uid`, `rootPath`, `pathPattern`, `segmenterUid`, `endpoints` | Schema metadata |

---

## Working with Nested Box Values

Python result contexts are returned as `Box` objects. Nested `ContextMap` values are also converted back into `Box` objects by the bindings, so nested attribute access works consistently.

#### Python

```python
from silex import Box

ctx = Box(project=Box(code="bob"), entity=Box(name="chrBob"))

print(ctx.project.code)          # "bob"
print(ctx["project"].code)      # "bob"
print(ctx.get("missing", "UNK"))

result = resolver.context_from_path(path)
if result.context.project:
    print(result.context.project.code)

nested_dict = result.context.to_dict()
```

This matches the nested `Box(project=Box(code="bob"))` patterns exercised in the unit tests.

#### C++

```cpp
#include <silex/Box.h>

silex::Box ctx(result.context);

auto projectCode = ctx.get<std::string>("project.code", "UNK");
auto projectBox = ctx.box("project");
auto projectCodeAgain = projectBox.get<std::string>("code", "UNK");
```

Prefer dot-path access when you only need a single value, and `box()` when you want to pass around a nested subtree.

---

## Working with Results

### Inspecting context_from_path Results

#### Python

```python
result = resolver.context_from_path(path)

if result.status == ResolverStatus.SUCCESS:
    # Context access is via Box attribute-style notation
    ctx = result.context
    print(ctx.project)                    # top-level key
    print(ctx.entity.fragment.version)    # nested key
    print(f"Schema: {result.schema_uid}")
    print(f"Endpoint: {result.schema_endpoint}")
elif result.status == ResolverStatus.PARTIAL:
    print(f"Partial match up to: {result.resolved_path}")
    print(f"Unresolved: {result.unresolved_path}")
```

#### C++

```cpp
auto result = resolver.contextFromPath(path);

if (silex::hasFlag(result.status, silex::ResolverStatus::Success)) {
    silex::Box ctx(result.context);
    auto project = ctx.get<std::string>("project");
    // result.schemaUid, result.schemaEndpoint, etc.
}
```

### Inspecting path_from_context Results

#### Python

```python
result = resolver.path_from_context(context, endpoint=["component"])

if result.status == ResolverStatus.SUCCESS:
    print(result.resolved_path)
else:
    print(f"Missing keys: {result.missing_context}")
    print(f"Furthest segment: {result.furthest_segment}")
```

If `result.matches` contains more than one successful candidate, the status also includes
the ambiguous flag. Python callers can inspect `matches` to see every surviving candidate
in resolver order.

#### C++

```cpp
auto result = resolver.pathFromContext(context, {"component"});

if (silex::hasFlag(result.status, silex::ResolverStatus::Success)) {
    std::cout << result.resolvedPath << std::endl;
} else {
    // result.missingContext lists keys that were needed
}
```

---

## Templates

Templates are named value patterns defined in `.silex` schema files. They provide parse and format expressions for structured string values.

### Listing Templates

```python
templates = resolver.available_templates()
# ["version_tag", "entity_name", ...]
```

```cpp
auto templates = resolver.availableTemplates();
```

### Extracting Context from a Value

```python
ctx = resolver.parse_template_value("v003", "version_tag")
# {"version": 3}
```

```cpp
auto ctx = resolver.parseTemplateValue("v003", "version_tag");
```

### Generating a Value from Context

```python
value = resolver.format_template_value({"version": 3}, "version_tag")
# "v003"
```

```cpp
silex::ContextMap context;
context["version"] = std::any(3);
auto value = resolver.formatTemplateValue(context, "version_tag");
```

---

## Advanced: PlaceholderValue

`PlaceholderValue` lets you generate glob-style paths when some context values are unknown. Instead of providing a concrete value, supply a `PlaceholderValue` with a wildcard pattern. The resolver substitutes the placeholder into the output path.

### Python

```python
from silex import Placeholder

context = {
    "project": "PROJ",
    "entity": Placeholder("*"),  # Wildcard — matches any entity
    "fragment": "model"
}
result = resolver.path_from_context(context, endpoint=["fragment"])
# result.resolved_path contains a glob path with * where entity should be
# e.g. "/projects/PROJ/assets/*/model"
```

### C++

```cpp
#include <silex/types.h>

silex::ContextMap context;
context["project"] = std::any(std::string("PROJ"));
context["entity"] = std::any(silex::PlaceholderValue{"*"});
context["fragment"] = std::any(std::string("model"));

auto result = resolver.pathFromContext(context, {"fragment"});
// result.resolvedPath contains the glob path
```

This is useful for building file-search patterns when you want to discover all entities, versions, or other variable path components on disk.
