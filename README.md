# Silex Path Resolver

**Silex** is a graph-based path resolution engine for VFX and animation pipelines. It bidirectionally converts between filesystem paths, URIs, and structured context dictionaries using schema-driven segment trees and pluggable functor expressions.

The runtime is implemented as a C++17 core library with pybind11 bindings. Python consumers install prebuilt wheels from PyPI and import the compiled `silex` module, while C++ consumers can still build against `silex_core` and the public headers in `include/silex`.

## Installation

Install the published package with pip:

```bash
pip install silex
```

If you are building from source locally, use a normal Python build frontend:

```bash
python -m pip install --upgrade pip build
python -m build --wheel
```

## Quick Start

```python
import os
os.environ["SILEX_SCHEMA_PATH"] = "/path/to/schemas"

from silex import GenericResolver, ResolverStatus, set_verbosity, Verbosity

set_verbosity(Verbosity.Info)
resolver = GenericResolver()

# Path → Context
result = resolver.context_from_path("/projects/PROJ/assets/chrBob/model/v001/scene.ma")
if result.status == ResolverStatus.SUCCESS:
	print(result.context.project)

# Context → Path
result = resolver.path_from_context({"project": "PROJ", "entity": "chrBob", "fragment": "model"})
print(result.resolved_path)
```

## Build

Build wheels and source distributions with standard Python tooling:

```bash
python -m pip install --upgrade pip build
python -m build
```

GitHub Actions builds wheels for Windows, Linux, and macOS, publishes tagged releases to PyPI, and publishes the generated Doxygen site to GitHub Pages.

The build also generates packaged API documentation from the public headers and documentation sources using Doxygen.

The install layout is split by consumer:

- `python/` contains the compiled `silex` extension module and any required runtime DLLs.
- `include/`, `lib/`, and `bin/` contain the public C++ headers, import library, and runtime library.
- `docs/` contains packaged manuals and generated API documentation.

Shared production schemas now live in the `fr_env_config` package. Reference authoring examples live in `examples/`.

## Documentation

- [Documentation Index](docs/index.md) — Release-facing documentation map
- [Getting Started](docs/getting_started.md) — Installation, setup, and first resolver flows
- [API Guide](docs/api_guide.md) — User guide with Python and C++ examples
- [Schema Cookbook](docs/schema_cookbook.md) — Production patterns for filesystem and URI schemas
- [Examples](examples/README.md) — Standalone example schemas of varying complexity
- [Schema Authoring](docs/schema_authoring.md) — Comprehensive guide to writing `.silex` files
- [Build And Test](docs/build_and_test.md) — Local build, test, and packaging workflow
- [Troubleshooting](docs/troubleshooting.md) — Common resolver and schema authoring failure modes
- [Generated HTML API Docs](docs/html/index.html) — Doxygen-generated packaged API site
- [Architecture](docs/architecture.md) — Developer guide with diagrams and internals
- [Architecture Decision Records](docs/architecture-decision-records/) — Design decisions and rationale

## License

This project is licensed under the MIT License. See `LICENSE`.
