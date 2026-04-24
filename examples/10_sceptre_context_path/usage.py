# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Demonstrate compact sceptre context paths for both shot and asset views."""

from silex import GenericResolver, ResolverStatus


def main() -> None:
    """Verify compact context paths can be written and parsed for multiple branches."""
    resolver = GenericResolver(schema="example.dir.sceptre_context")

    # Write a shot-oriented context path and confirm the switch branch reads back correctly.
    shot_result = resolver.path_from_context(
        {"context": {"project": "PROJ1", "tree": "shots", "sequence": "seq010", "shot": "sh020"}},
        endpoint="sceptre.context.shot",
    )
    assert shot_result.status == ResolverStatus.SUCCESS
    assert shot_result.resolved_path.replace("\\", "/") == "/PROJ1/shots/seq010/sh020"

    read_shot = resolver.context_from_path("/PROJ1/shots/seq010/sh020", endpoint="sceptre.context.shot")
    assert read_shot.status == ResolverStatus.SUCCESS
    assert read_shot.context["context"]["tree"] == "shots"
    assert read_shot.context["context"]["shot"] == "sh020"

    # Write an asset-oriented context path so both switch branches stay covered.
    asset_result = resolver.path_from_context(
        {"context": {"project": "PROJ1", "tree": "assets", "asset_variant": "chrHeroMain"}},
        endpoint="sceptre.context.asset_variant",
    )
    assert asset_result.status == ResolverStatus.SUCCESS
    assert asset_result.resolved_path.replace("\\", "/") == "/PROJ1/assets/chrHeroMain"

    read_asset = resolver.context_from_path("/PROJ1/assets/chrHeroMain", endpoint="sceptre.context.asset_variant")
    assert read_asset.status == ResolverStatus.SUCCESS
    assert read_asset.context["context"]["asset_variant"] == "chrHeroMain"


if __name__ == "__main__":
    main()