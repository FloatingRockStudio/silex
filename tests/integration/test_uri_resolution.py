"""
Integration tests for URI resolution with partitions.

Tests FRA:// URI resolution patterns aligned with fr_asset_api usage:
- Asset and shot context switching
- Query parameter partitions (version, tags, frame, udim, ext)
- Bidirectional resolution (context -> URI -> context)
- Type conversion in partitions
"""

import os
from pathlib import Path

import pytest
from silex import GenericResolver, ResolverStatus


@pytest.fixture(autouse=True)
def setup_schema_path(monkeypatch):
    """Set schema path for all tests in this module."""
    test_resources = Path(__file__).parent.parent / "resources"
    monkeypatch.setenv("SILEX_SCHEMA_PATH", str(test_resources))


class TestURIContextResolution:
    """Test URI resolution for different context types."""

    def test_asset_entity_with_query_params(self):
        """Test asset entity URI with full query parameters."""
        resolver = GenericResolver()
        uri = "FRA://asset_entity/BLU/chrbobBilbyBase/texture/diffuse?v=5&udim=1001&ext=exr&tag=latest&tag=published"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["uri"]["context"] == "asset_entity"
        assert result.context["context"]["project"] == "BLU"
        assert result.context["context"]["entity"] == "chrbobBilbyBase"
        assert result.context["context"]["fragment"] == "texture"
        assert result.context["context"]["component"] == "diffuse"
        assert result.context["options"]["version"] == 5
        assert result.context["options"]["udim"] == "1001"
        assert result.context["options"]["ext"] == "exr"
        assert result.context["options"]["tags"] == ["latest", "published"]

    def test_shot_with_frame_param(self):
        """Test shot URI with frame query parameter."""
        resolver = GenericResolver()
        uri = "FRA://shot/ABC123/seq010/shot050?frame=1234&task=animation"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["uri"]["context"] == "shot"
        assert result.context["context"]["project"] == "ABC123"
        assert result.context["context"]["sequence"] == "seq010"
        assert result.context["context"]["shot"] == "shot050"
        assert result.context["options"]["frame"] == 1234

    def test_uri_without_query_params(self):
        """Test URI resolution without query parameters."""
        resolver = GenericResolver()
        uri = "FRA://asset_entity/XYZ/testAsset/geometry/lowres"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["context"]["component"] == "lowres"
        assert "options" not in result.context or not result.context.get("options")


class TestBidirectionalResolution:
    """Test roundtrip context <-> URI resolution."""

    def test_asset_roundtrip(self):
        """Test context to URI to context roundtrip for assets."""
        resolver = GenericResolver()
        context = {
            "uri": {"context": "asset_entity"},
            "context": {
                "project": "BLU",
                "entity": "chrbobBilbyBase",
                "fragment": "texture",
                "component": "diffuse",
            },
            "options": {"tags": ["latest", "published"], "version": 5, "udim": "1001", "ext": "exr"},
        }

        result = resolver.path_from_context(context, endpoint="asset_entity", include_children=True)
        assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)
        expected = "FRA://asset_entity/BLU/chrbobBilbyBase/texture/diffuse?v=5&udim=1001&ext=exr&tag=latest&tag=published"
        assert result.resolved_path == expected

    def test_sequence_roundtrip(self):
        """Test context to URI roundtrip for sequences."""
        resolver = GenericResolver()
        context = {
            "uri": {"context": "sequence"},
            "context": {"project": "PROJ", "sequence": "seq042"},
            "options": {"version": 7, "status": "review"},
        }

        result = resolver.path_from_context(context, endpoint="sequence", include_children=True)
        assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)
        assert "FRA://sequence/PROJ/seq042?v=7&status=review" == result.resolved_path


class TestPartitionBehavior:
    """Test query parameter partition handling."""

    def test_multiple_tags_as_array(self):
        """Test multiple tag parameters are collected as array."""
        resolver = GenericResolver()
        uri = "FRA://asset_component/GAME/weapon/texture/normal?tag=approved&tag=final&tag=v2"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["options"]["tags"] == ["approved", "final", "v2"]

    def test_type_conversion(self):
        """Test type conversion in partitions."""
        resolver = GenericResolver()
        uri = "FRA://sequence/PROJ/seq123?v=999&status=draft"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert isinstance(result.context["options"]["version"], int)
        assert result.context["options"]["version"] == 999
        assert isinstance(result.context["options"]["status"], str)

    def test_unknown_params_ignored(self):
        """Test unrecognized query params don't break resolution."""
        resolver = GenericResolver()
        uri = "FRA://asset_entity/PRJ/test/tex/diff?unknown=value&invalid=param"
        result = resolver.context_from_path(uri)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["context"]["project"] == "PRJ"
        assert "unknown" not in result.context.get("options", {})


class TestErrorHandling:
    """Test error cases in URI resolution."""

    def test_invalid_context_type(self):
        """Test handling of invalid URI context types."""
        resolver = GenericResolver()
        uri = "FRA://invalid_context/incomplete"
        result = resolver.context_from_path(uri)
        assert result.status == ResolverStatus.ERROR


@pytest.mark.parametrize(
    "uri_context,project,segments,query",
    [
        ("asset_entity", "PROJ1", "entity1/tex/diff", "v=1"),
        ("asset_component", "PROJ2", "entity2/model/hires", "v=12&ext=abc"),
        ("sequence", "PROJ3", "seq001", "v=3"),
        ("shot", "PROJ4", "seq001/shot001", "frame=100"),
    ],
)
def test_uri_pattern_variations(uri_context, project, segments, query):
    """Test various URI pattern combinations."""
    resolver = GenericResolver()
    uri = f"FRA://{uri_context}/{project}/{segments}?{query}"
    result = resolver.context_from_path(uri)

    assert result.status == ResolverStatus.SUCCESS
    assert result.context["uri"]["context"] == uri_context
    assert result.context["context"]["project"] == project


@pytest.mark.parametrize(
    "param_string,expected",
    [
        ("v=1&ext=jpg", {"version": 1, "ext": "jpg"}),
        ("udim=1001&tag=final", {"udim": "1001", "tags": ["final"]}),
        ("tag=a&tag=b&tag=c", {"tags": ["a", "b", "c"]}),
        ("v=42", {"version": 42}),
    ],
)
def test_query_param_combinations(param_string, expected):
    """Test various query parameter combinations."""
    resolver = GenericResolver()
    uri = f"FRA://asset_entity/TEST/entity/frag/comp?{param_string}"
    result = resolver.context_from_path(uri)

    assert result.status == ResolverStatus.SUCCESS
    for key, value in expected.items():
        assert result.context["options"][key] == value
