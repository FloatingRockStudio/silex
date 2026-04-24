"""Demonstrate a schema-declared custom functor UID backed by a registered Python functor instance."""

from __future__ import annotations

import sys
from pathlib import Path

from silex import GenericResolver, Language, ResolverStatus, SilexFunctorInfo
from silex.core import FileSchemaLoader, Registry

EXAMPLE_DIR = Path(__file__).parent
if str(EXAMPLE_DIR) not in sys.path:
    sys.path.insert(0, str(EXAMPLE_DIR))

from custom_functors import build_lower_slug_functor


def main() -> None:
    """Register a project-local custom functor UID and verify the resolver can use it."""
    config_id = "example_custom_functor"
    registry = Registry.instance(config_id)

    # Schema preload will register the metadata too; this preserves the live Python factory.
    registry.register_functor(
        SilexFunctorInfo(
            uid="example.custom.sluglower",
            name="LowerSlugFunctor",
            module="custom_functors",
            package="",
            language=Language.PYTHON,
            aliases=["sluglower"],
        ),
        build_lower_slug_functor(registry),
    )

    # Preload the examples so the schema declaration itself is exercised too.
    loader = FileSchemaLoader(registry)
    loader.preload()
    schema_info = loader.load_info("example.dir.custom_functor")
    assert schema_info.uid == "example.dir.custom_functor"
    assert registry.get_functor_info("sluglower") is not None

    resolver = GenericResolver(schema="example.dir.custom_functor", config_id=config_id)
    expected_path = "/show/custom_functor/heroasset/v002"

    # Write first so the schema demonstrates a custom registered functor driving path output.
    write_result = resolver.path_from_context(
        {"context": {"asset": "HeroAsset"}, "task": {"version": 2}},
        endpoint="asset.version",
    )
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path

    # Read the same path back so the custom registration remains executable documentation.
    read_result = resolver.context_from_path(expected_path, endpoint="asset.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["asset"] == "heroasset"
    assert read_result.context["task"]["version"] == "002"


if __name__ == "__main__":
    main()