# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Demonstrate a client-specific publish variant with a customized version token."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify the client-specific publish path stays round-trip safe."""
    resolver = GenericResolver(schema="example.dir.client_publish")
    expected_path = "/show/publish/PROJ1/hero/publish/ver_014"

    # Write first so the example makes the client-specific output obvious.
    write_result = resolver.path_from_context(
        {"context": {"project": "PROJ1", "asset": "hero"}, "file": {"version": 14}},
        endpoint="asset.publish.version",
    )
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path

    # Read the client-specific path back to prove the specialized layout is symmetrical.
    read_result = resolver.context_from_path(expected_path, endpoint="asset.publish.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["file"]["version"] == "014"
    assert read_result.context["context"]["asset"] == "hero"


if __name__ == "__main__":
    main()