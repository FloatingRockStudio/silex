import os
from pathlib import Path

import pytest

from silex import GenericResolver, PlaceholderValue, ResolverStatus, SilexParseOptions


@pytest.fixture(autouse=True)
def configure_schema(monkeypatch):
    resources = Path(__file__).resolve().parent.parent / "resources"
    monkeypatch.setenv("SILEX_SCHEMA_PATH", str(resources))


@pytest.fixture
def resolver():
    options = SilexParseOptions(schema="test.switch.placeholder")
    instance = GenericResolver(options)
    return instance


def test_switch_endpoint_resolution(resolver):
    context = {
        "context": {
            "classification": "chr",
            "asset_variant": "chrHero",
        }
    }

    result = resolver.path_from_context(
        context,
        endpoint="asset.scene",
        schema="test.switch.placeholder",
    )

    assert result.status == ResolverStatus.SUCCESS
    assert result.schema_endpoint == "asset.scene"
    assert result.resolved_path.endswith(os.path.join("Assets", "chr", "chrHero", "Scenefiles"))


def test_missing_classification_stops_at_partial_root(resolver):
    context = {
        "context": {
            "asset_variant": "chrHero",
        }
    }

    result = resolver.path_from_context(
        context,
        endpoint="asset.entity",
        schema="test.switch.placeholder",
    )

    assert result.status == ResolverStatus.PARTIAL
    assert result.resolved_path == "/project"
    assert result.context["context"] == {"asset_variant": "chrHero"}


def test_placeholder_generates_wildcard(resolver):
    context = {
        "context": {
            "classification": PlaceholderValue("*"),
            "asset_variant": PlaceholderValue("*"),
        },
        "entity": {
            "fragment": "geo",
            "fragment_version": PlaceholderValue("*"),
        }
    }

    result = resolver.path_from_context(
        context,
        endpoint="asset.fragment.version",
        schema="test.switch.placeholder",
    )

    assert result.status == ResolverStatus.SUCCESS
    assert "*" in result.resolved_path
    assert result.resolved_path.endswith(os.path.join("Entity", "geo", "v*"))


def test_placeholder_asset_variant_without_classification_stays_partial(resolver):
    context = {
        "context": {
            "asset_variant": PlaceholderValue("*"),
        }
    }

    result = resolver.path_from_context(
        context,
        endpoint="asset.entity",
        schema="test.switch.placeholder",
    )

    assert result.status == ResolverStatus.PARTIAL
    assert result.resolved_path == "/project"
    assert str(result.context["context"]["asset_variant"]) == "*"
