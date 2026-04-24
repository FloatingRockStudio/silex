"""Tests for resolve struct behavior via the C++ bindings."""

import json

from silex import Box, ResolverStatus, GenericResolver, SilexParseOptions


def test_parse_options_include_deprecated_defaults_false():
    options = SilexParseOptions()

    assert options.include_deprecated is False


def test_parse_options_include_deprecated_can_be_set():
    options = SilexParseOptions(include_deprecated=True)

    assert options.include_deprecated is True


def test_resolver_accepts_options_kwarg():
    resolver = GenericResolver(
        options=SilexParseOptions(include_deprecated=True),
        schema="test.case_sensitivity",
    )
    # Resolver should be constructed without error.
    assert resolver is not None


def test_box_is_json_serializable():
    payload = Box({"project": Box({"code": "POE2"}), "version": 1})

    assert json.loads(json.dumps(payload)) == {
        "project": {"code": "POE2"},
        "version": 1,
    }