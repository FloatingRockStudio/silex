# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""
Regression tests for switch-branch selection when writing paths.

When two switch branches end in the same endpoint uid, writing a path must
traverse the branch selected by the written switch value. Historically the
writer tried every branch and the first one to reach the endpoint won,
producing hybrid paths — e.g. a ``shot_fragment`` URI continuing with the
``asset_fragment`` chain, silently dropping the sequence/shot segments.
"""

from pathlib import Path

import pytest
from silex import GenericResolver, ResolverStatus


@pytest.fixture(autouse=True)
def setup_schema_path(monkeypatch):
    """Set schema path for all tests in this module."""
    test_resources = Path(__file__).parent.parent / "resources"
    monkeypatch.setenv("SILEX_SCHEMA_PATH", str(test_resources))


SCHEMA = "test.switch.endpoint"


def _write(context):
    resolver = GenericResolver()
    return resolver.path_from_context(
        context, endpoint="component", schema=SCHEMA, include_children=True
    )


class TestSwitchBranchWrite:
    def test_shot_branch_keeps_its_own_segments(self):
        """The written switch value must pin traversal to its branch."""
        result = _write({
            "uri": {"context": "shot_fragment"},
            "context": {
                "project": "PROJ",
                "sequence": "sq010",
                "shot": "sh020",
                "entity": "chrHero_001",
                "component": "cache",
            },
        })

        assert result.status & ResolverStatus.SUCCESS
        assert result.resolved_path == (
            "TSW://shot_fragment/PROJ/sq010/sh020/chrHero_001/cache"
        )

    def test_asset_branch_unchanged(self):
        result = _write({
            "uri": {"context": "asset_fragment"},
            "context": {
                "project": "PROJ",
                "entity": "chrHero",
                "component": "geometry",
            },
        })

        assert result.status & ResolverStatus.SUCCESS
        assert result.resolved_path == "TSW://asset_fragment/PROJ/chrHero/geometry"

    def test_shot_write_roundtrips_through_parse(self):
        written = _write({
            "uri": {"context": "shot_fragment"},
            "context": {
                "project": "PROJ",
                "sequence": "sq010",
                "shot": "sh020",
                "entity": "chrHero_001",
                "component": "cache",
            },
        })
        assert written.status & ResolverStatus.SUCCESS

        resolver = GenericResolver()
        parsed = resolver.context_from_path(written.resolved_path)

        assert parsed.status & ResolverStatus.SUCCESS
        assert parsed.context["uri"]["context"] == "shot_fragment"
        assert parsed.context["context"]["sequence"] == "sq010"
        assert parsed.context["context"]["shot"] == "sh020"
        assert parsed.context["context"]["entity"] == "chrHero_001"
        assert parsed.context["context"]["component"] == "cache"
