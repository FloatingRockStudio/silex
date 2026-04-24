# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Demonstrate URI round-trips with ordered query parameter partitions."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Resolve query params into structured options and build them back again."""
    resolver = GenericResolver(schema="example.dir.uri")
    expected_uri = "FRA://asset/PROJ1/dragon?v=7&tag=final&tag=review&ext=exr"

    # Read the URI to show how partitioned query params populate structured options.
    read_result = resolver.context_from_path(expected_uri, endpoint="uri.entity")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["uri"]["resource_type"] == "asset"
    assert read_result.context["context"]["project"] == "PROJ1"
    assert read_result.context["context"]["entity"] == "dragon"
    assert read_result.context["options"]["version"] == 7
    assert read_result.context["options"]["tags"] == ["final", "review"]
    assert read_result.context["options"]["ext"] == "exr"

    # Write the parsed context back so the example covers URI generation as well.
    write_result = resolver.path_from_context(read_result.context, endpoint="uri.entity", include_children=True)
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path == expected_uri


if __name__ == "__main__":
    main()