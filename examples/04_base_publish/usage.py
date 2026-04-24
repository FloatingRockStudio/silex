# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Demonstrate a reusable publish tree that serves as an inheritance base."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify the base publish schema resolves and writes a canonical version path."""
    resolver = GenericResolver(schema="example.dir.base_publish")
    expected_path = "/show/publish/PROJ1/hero/publish/v014"

    # Read the canonical publish path so the inherited base contract is explicit.
    read_result = resolver.context_from_path(expected_path, endpoint="asset.publish.version")
    assert read_result.status == ResolverStatus.SUCCESS
    assert read_result.context["context"]["project"] == "PROJ1"
    assert read_result.context["context"]["asset"] == "hero"
    assert read_result.context["file"]["version"] == "014"

    # Write the endpoint back to keep the reusable base schema under test.
    write_result = resolver.path_from_context(read_result.context, endpoint="asset.publish.version")
    assert write_result.status == ResolverStatus.SUCCESS
    assert write_result.resolved_path.replace("\\", "/") == expected_path


if __name__ == "__main__":
    main()