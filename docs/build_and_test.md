# Build And Test

This document covers the expected local workflow for packaging and validating Silex.

The published PyPI distribution is `fr-silex`. Python code imports `silex` after installation.

## Python Package Build

Build the wheel and source distribution with standard Python tooling:

```bash
python -m pip install --upgrade pip build
python -m build
```

For a wheel-only local smoke build, this is sufficient:

```bash
python -m pip wheel --no-deps --wheel-dir dist .
```

## Rez Dev Packaging

When installing a local development build into Rez on Windows, build a wheel first and give that wheel to `rez-pip2`. Installing directly from the source tree can leave pip's temporary `_in_process.py` file locked on Windows during the PEP 517 metadata step.

Build the wheel with the target Rez Python:

```bash
rez-env python-3.9.7 -- python -m pip wheel --no-deps --no-build-isolation --wheel-dir dist .
```

Install the resulting wheel with `rez-pip2` from the `rez_pip` environment:

```bash
rez-env rez_pip -- rez-pip2 --python-version 3.9.7 "fr-silex @ file:///<absolute-repo-path>/dist/<wheel-file>.whl"
```

Validate the installed Rez package without adding local build directories to `PYTHONPATH`:

```bash
rez-env python-3.9.7 fr_silex -- python -c "import silex; print(silex.__file__)"
```

If the target Python fails with imports from another Python version, clear `PYTHONHOME` and `PYTHONPATH` before invoking `rez-pip2`. If `rez-pip2` reports `InvalidRequirement` for a raw `.whl` path, use the PEP 508 direct-reference form shown above instead of passing the Windows path directly. With `rez_pip` 0.4.2 on Windows, local wheel URLs may also be reported as `/H:/...`; normalize that to `H:/...` in a launcher script or update `rez_pip` before retrying.

For release validation, repeat the wheel build, Rez install, and import smoke test for every supported Python version listed in the packaging configuration.

## Build

Configure and build the native project directly with CMake when you need local C++ artifacts:

```bash
cmake -S . -B build/_cmake -DBUILD_TESTS=ON -DBUILD_PYTHON=ON
cmake --build build/_cmake --config Release
cmake --install build/_cmake --config Release --prefix build/package
```

The build performs these major steps:

1. configure and build the C++ core and Python bindings with CMake
2. install headers, libraries, and the Python extension into the chosen install tree
3. copy runtime DLLs into `python/` for import-time loading
4. generate Doxygen HTML documentation under `build/docs/html/`
5. install the built package contents into the final install root

## GitHub Actions

The repository includes three GitHub Actions workflows:

- `ci.yml` builds the source distribution and platform wheels on pushes and pull requests
- `publish.yml` publishes tagged releases to PyPI through trusted publishing
- `docs.yml` builds the Doxygen site and deploys it to GitHub Pages

When validating a release candidate locally, confirm that the built package metadata reports `fr-silex` and that `import silex` still succeeds.

## Tests

The package defines two primary test suites:

- core C++ tests via `bin/silex_core_tests`
- Python tests via `python -m pytest tests/ -v`

For the wheel smoke test, install the built wheel into a clean target directory and verify that `import silex` succeeds.

Run the broader repository tests from the repository root after building.

## Documentation Output

The CMake install flow and GitHub Actions docs workflow generate package documentation into `build/docs/html/`.

That output includes:

- a Doxygen HTML site generated from the public headers and documentation sources
- rendered versions of the Markdown documentation under `docs/`

## Release Validation

Before cutting a release, verify:

- the package imports successfully in Python
- the built distribution is named `fr-silex`
- resolver examples still work against bundled schemas
- `build/docs/html/index.html` exists
- the generated site includes the public headers you expect to ship