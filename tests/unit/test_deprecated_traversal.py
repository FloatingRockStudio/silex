import json
import os
import uuid
from pathlib import Path

import pytest

from silex import GenericResolver, ResolverStatus, SilexParseOptions


def _build_deprecated_segment_schema(root_path: Path) -> dict:
    return {
        "uid": "test.deprecated_traversal",
        "segmenter": "silex.impl.segmenters.FilesystemSegmenter",
        "pattern": ".*",
        "root_path": str(root_path),
        "paths": {
            "project": {
                "pattern": "([A-Z0-9]+)",
                "target": "context.project",
                "paths": {
                    "legacy": {
                        "pattern": "legacy",
                        "is_deprecated": True,
                        "paths": {
                            "asset": {
                                "pattern": "asset_(.+)",
                                "target": "context.asset",
                                "endpoint": "asset",
                            }
                        },
                    }
                },
            }
        },
    }


@pytest.fixture
def deprecated_schema_dir(tmp_path, monkeypatch):
    resources_path = Path(__file__).parent.parent / "resources"
    schema_dir = tmp_path / "schemas"
    schema_dir.mkdir()
    data_dir = tmp_path / "data"
    data_dir.mkdir()

    schema_path = schema_dir / "test_deprecated_traversal.silex"
    schema_path.write_text(json.dumps(_build_deprecated_segment_schema(data_dir), indent=2), encoding="utf-8")

    monkeypatch.setenv("SILEX_SCHEMA_PATH", os.pathsep.join([str(resources_path), str(schema_dir)]))

    yield {"schema_dir": schema_dir, "data_dir": data_dir}


def _resolver(schema: str, include_deprecated: bool = False, allow_partial: bool = False) -> GenericResolver:
    return GenericResolver(
        options=SilexParseOptions(include_deprecated=include_deprecated, allow_partial=allow_partial),
        schema=schema,
        config_id=f"deprecated_traversal_{uuid.uuid4().hex}",
    )


def test_deprecated_segment_excluded_by_default(deprecated_schema_dir):
    resolver = _resolver("test.deprecated_traversal")
    deprecated_path = str(deprecated_schema_dir["data_dir"] / "PRJ" / "legacy" / "asset_hero")

    result = resolver.context_from_path(deprecated_path, endpoint="asset")

    assert result.status == ResolverStatus.ERROR


def test_deprecated_segment_skipped_by_default_for_canonical_read_path(deprecated_schema_dir):
    resolver = _resolver("test.deprecated_traversal")
    canonical_path = str(deprecated_schema_dir["data_dir"] / "PRJ" / "asset_hero")

    result = resolver.context_from_path(canonical_path, endpoint="asset")

    assert result.status == ResolverStatus.SUCCESS
    assert len(result.matches) == 1
    assert result.matches[0].schema_endpoint_path == "project.asset"
    assert result.matches[0].used_deprecated_traversal is True
    assert result.context["context"]["project"] == "PRJ"
    assert result.context["context"]["asset"] == "asset_hero"


def test_deprecated_segment_included_when_requested(deprecated_schema_dir):
    resolver = _resolver("test.deprecated_traversal", include_deprecated=True)
    deprecated_path = str(deprecated_schema_dir["data_dir"] / "PRJ" / "legacy" / "asset_hero")

    result = resolver.context_from_path(deprecated_path, endpoint="asset")

    assert result.status == ResolverStatus.SUCCESS
    assert len(result.matches) == 1
    assert result.matches[0].schema_endpoint_path == "project.legacy.asset"
    assert result.matches[0].used_deprecated_traversal is True
    assert result.context["context"]["asset"] == "asset_hero"


def test_path_generation_excludes_deprecated_routes_by_default(deprecated_schema_dir):
    resolver = _resolver("test.deprecated_traversal")
    context = {"context": {"project": "PRJ", "asset": "asset_hero"}}

    result = resolver.path_from_context(context, endpoint="asset")

    assert result.status == ResolverStatus.SUCCESS
    assert len(result.matches) == 1
    assert result.resolved_path == str(deprecated_schema_dir["data_dir"] / "PRJ" / "asset_hero")
    assert result.matches[0].schema_endpoint_path == "project.asset"
    assert result.matches[0].used_deprecated_traversal is True


def test_path_generation_reports_canonical_and_deprecated_matches_when_requested(deprecated_schema_dir):
    resolver = _resolver("test.deprecated_traversal", include_deprecated=True)
    context = {"context": {"project": "PRJ", "asset": "asset_hero"}}

    result = resolver.path_from_context(context, endpoint="asset")

    canonical_path = str(deprecated_schema_dir["data_dir"] / "PRJ" / "asset_hero")
    deprecated_path = str(deprecated_schema_dir["data_dir"] / "PRJ" / "legacy" / "asset_hero")

    assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)
    assert [match.resolved_path for match in result.matches] == [canonical_path, deprecated_path]
    assert [match.schema_endpoint_path for match in result.matches] == ["project.asset", "project.legacy.asset"]
    assert all(match.used_deprecated_traversal for match in result.matches)
