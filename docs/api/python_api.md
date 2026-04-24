# Python API Reference

The packaged documentation site is generated with Doxygen during the standard CMake and release build flow.

For Python consumers, the primary public surface is the `silex` pybind11 module documented in:

- [API Guide](../api_guide.md) for usage-oriented examples
- the binding source pages generated from `source/silex_core/bindings`

The release-facing Python entry points include:

- `GenericResolver`
- `Box`
- `SilexParseOptions`
- `SilexContextResolve`
- `SilexPathResolve`
- exported enums such as `ResolverStatus` and `Verbosity`