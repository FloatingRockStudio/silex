# Getting Started

Silex resolves filesystem paths and URIs into structured context, and can generate those paths again from context using `.silex` schemas.

## Installation

Install the published package with pip:

```bash
pip install fr-silex
```

The PyPI distribution is named `fr-silex`, but Python code imports `silex`.

Build and install from source:

```bash
python -m pip install --upgrade pip build
python -m build
```

The build installs:

- `python/` for the compiled Python module and runtime DLLs
- `include/`, `lib/`, and `bin/` for C++ consumers
- `docs/` for packaged manual and generated API documentation
- `examples/` in source control for reference schemas and authoring patterns

## Environment Setup

Silex discovers schema files from `SILEX_SCHEMA_PATH`. Provide one or more schema roots through your environment package, such as `fr_env_config` for shared production schemas, and add project-specific schema roots as needed.

Python consumers import the compiled `silex` module:

```python
from silex import GenericResolver, ResolverStatus

resolver = GenericResolver()
result = resolver.context_from_path("/projects/PROJ/assets/chrBob/model/v001/scene.ma")

if result.status == ResolverStatus.SUCCESS:
    print(result.context.project)
```

## Core Workflow

Use `context_from_path(...)` when you need to parse a path or URI into named context values.

Use `path_from_context(...)` when you already have context and need the canonical output path for a schema endpoint.

Templates are the third core entry point. They let you parse or render a single value without traversing a full schema tree.

## Next Reading

- [API Guide](api_guide.md) for resolver usage details
- [Schema Authoring](schema_authoring.md) for the schema language
- [Schema Cookbook](schema_cookbook.md) for production examples
- [API Reference](api/index.md) for generated symbol-level reference