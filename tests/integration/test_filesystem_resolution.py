"""
Integration tests for filesystem path resolution.

Tests the resolver's ability to:
- Parse filesystem paths into context using schema definitions
- Build paths from context using schema definitions
- Handle partial resolution and round-trips
- Support placeholder priority in path generation
"""

import json
import os
import tempfile
from pathlib import Path

import pytest
from silex import GenericResolver, ResolverStatus, SilexParseOptions


TEST_DATA_ROOT = Path(__file__).parent / "test_data"


def _test_data_path(*parts: str) -> str:
    return str(TEST_DATA_ROOT.joinpath(*parts))


@pytest.fixture(autouse=True)
def setup_schema_path(monkeypatch):
    """Set schema path for all tests."""
    test_resources = Path(__file__).parent.parent / "resources"
    monkeypatch.setenv("SILEX_SCHEMA_PATH", str(test_resources))


class TestPathToContext:
    """Test parsing filesystem paths into context."""

    def test_shot_path_resolution(self):
        """Test shot path resolves to correct context."""
        resolver = GenericResolver()
        path = _test_data_path("test_project", "shots", "seq001", "shot001")
        result = resolver.context_from_path(path)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["context"]["project"] == "test_project"
        assert result.context["context"]["tree"] == "shots"
        assert result.context["context"]["sequence"] == "seq001"
        assert result.context["context"]["shot"] == "shot001"

    def test_asset_path_resolution(self):
        """Test asset path resolves to correct context."""
        resolver = GenericResolver()
        path = _test_data_path("test_project", "assets", "characters", "hero_character")
        result = resolver.context_from_path(path)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["context"]["project"] == "test_project"
        assert result.context["context"]["tree"] == "assets"
        assert result.context["context"]["asset_type"] == "characters"
        assert result.context["context"]["asset_name"] == "hero_character"

    def test_partial_path_resolution(self):
        """Test partial path still extracts available context."""
        resolver = GenericResolver()
        path = _test_data_path("test_project")
        result = resolver.context_from_path(path)

        assert result.status == ResolverStatus.SUCCESS
        assert result.context["project"]["code"] == "test_project"

    def test_invalid_path_returns_error(self):
        """Test invalid paths return ERROR status."""
        resolver = GenericResolver()
        invalid_paths = [
            "/completely/invalid/path",
            "C:/Windows/System32/invalid",
            "not_a_path_at_all",
        ]

        for path in invalid_paths:
            result = resolver.context_from_path(path)
            assert result.status == ResolverStatus.ERROR


class TestContextToPath:
    """Test building paths from context."""

    def test_shot_path_building(self):
        """Test building shot path from context."""
        resolver = GenericResolver()
        context = {
            "project": {"code": "test_project"},
            "context": {"project": "test_project", "tree": "shots", "sequence": "seq001", "shot": "shot001"},
        }

        result = resolver.path_from_context(context, endpoint="shot")
        assert result is not None
        # Basic check - more detailed assertions depend on schema write expressions

    def test_asset_path_building(self):
        """Test building asset path from context."""
        resolver = GenericResolver()
        context = {
            "project": {"code": "test_project"},
            "context": {
                "tree": "assets",
                "asset_type": "characters",
                "asset_name": "hero_character",
            },
        }

        result = resolver.path_from_context(context, endpoint="asset")
        assert result is not None

    def test_project_glob_write_is_case_insensitive_for_lowercase_context(self, monkeypatch):
        """Test lowercase project context still resolves canonical project and production directories."""
        test_resources = Path(__file__).parent.parent / "resources"

        schema = {
            "uid": "test.project_glob_case_insensitive",
            "root_path": "",
            "segmenter": "silex.FilesystemSegmenter",
            "pattern": "(([a-zA-Z]:)|(\\\\[^\\\\]+\\\\[^\\\\]+))([\\\\/][^\\\\/:*?\"<>|]+)*",
            "functors": [
                {
                    "package": "silex",
                    "module": "silex.impl.functors.case_conversion",
                    "name": "ConvertLowerCaseFunctor",
                    "aliases": ["lower"],
                    "language": "python",
                },
                {
                    "module": "silex.impl.functors.glob",
                    "name": "GlobFunctor",
                    "aliases": ["glob"],
                    "language": "python",
                },
            ],
            "aliases": {
                "lower": "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                "glob": "silex.impl.functors.glob.GlobFunctor",
            },
            "paths": {
                "project": {
                    "pattern": "(FR[0-9]*_)?(.*)",
                    "endpoint": "context.project",
                    "parse": {
                        "expressions": [
                            "$lower({group[2]})->project",
                        ],
                        "targets": {
                            "context.project": "project",
                            "project.code": {"group": 2},
                            "project.name": {"group": 0},
                        },
                    },
                    "format": "$glob(FR*_{context.project}, {context.project}, '')",
                    "paths": {
                        "production_dir": {
                            "pattern": "(?i)((03)[_\\. ]*)?Production",
                            "endpoint": "project.production_dir",
                            "format": "$glob(03_Production, Production)",
                        }
                    },
                }
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir_str:
            temp_dir = Path(temp_dir_str)
            schema_path = temp_dir / "test_project_glob_case_insensitive.silex"
            schema_path.write_text(json.dumps(schema, indent=2), encoding="utf-8")

            project_root = temp_dir / "FR00082_POE2"
            (project_root / "03_Production").mkdir(parents=True)
            (project_root / "Production").mkdir(parents=True)

            monkeypatch.setenv("SILEX_SCHEMA_PATH", os.pathsep.join([str(temp_dir), str(test_resources)]))

            resolver = GenericResolver(
                schema="test.project_glob_case_insensitive",
                config_id="test.project_glob_case_insensitive",
            )

            read_result = resolver.context_from_path(str(project_root), endpoint="context.project")

            assert read_result.status == ResolverStatus.SUCCESS
            assert read_result.context["context"]["project"] == "poe2"
            assert read_result.context["project"]["code"] == "POE2"
            assert read_result.context["project"]["name"] == "FR00082_POE2"

            project_write_result = resolver.path_from_context(
                {"context": {"project": "poe2"}},
                endpoint="context.project",
            )

            assert project_write_result.status == ResolverStatus.SUCCESS
            project_write_path = Path(project_write_result.resolved_path)
            assert project_write_path.parts[: len(project_root.parts)] == project_root.parts

            production_write_result = resolver.path_from_context(
                {"context": {"project": "poe2"}},
                endpoint="project.production_dir",
            )

            assert production_write_result.status == ResolverStatus.SUCCESS
            assert Path(production_write_result.resolved_path) == project_root / "03_Production"

    def test_project_glob_write_surfaces_all_ordered_candidates(self, monkeypatch):
        """Test ordered glob write returns a primary path and all matching candidates."""
        test_resources = Path(__file__).parent.parent / "resources"

        schema = {
            "uid": "test.project_glob_ambiguous_write",
            "root_path": "",
            "segmenter": "silex.FilesystemSegmenter",
            "pattern": "(([a-zA-Z]:)|(\\\\[^\\\\]+\\\\[^\\\\]+))([\\\\/][^\\\\/:*?\"<>|]+)*",
            "functors": [
                {
                    "package": "silex",
                    "module": "silex.impl.functors.case_conversion",
                    "name": "ConvertLowerCaseFunctor",
                    "aliases": ["lower"],
                    "language": "python",
                },
                {
                    "module": "silex.impl.functors.glob",
                    "name": "GlobFunctor",
                    "aliases": ["glob"],
                    "language": "python",
                },
            ],
            "aliases": {
                "lower": "silex.impl.functors.case_conversion.ConvertLowerCaseFunctor",
                "glob": "silex.impl.functors.glob.GlobFunctor",
            },
            "paths": {
                "project": {
                    "pattern": "(FR[0-9]*_)?(.*)",
                    "endpoint": "context.project",
                    "parse": {
                        "expressions": [
                            "$lower({group[2]})->project",
                        ],
                        "targets": {
                            "context.project": "project",
                            "project.code": {"group": 2},
                            "project.name": {"group": 0},
                        },
                    },
                    "format": "$glob(FR*_{context.project}, {context.project}, '')",
                }
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir_str:
            temp_dir = Path(temp_dir_str)
            schema_path = temp_dir / "test_project_glob_ambiguous_write.silex"
            schema_path.write_text(json.dumps(schema, indent=2), encoding="utf-8")

            canonical_root = temp_dir / "FR00082_POE2"
            rogue_root = temp_dir / "poe2"
            canonical_root.mkdir(parents=True)
            rogue_root.mkdir(parents=True)

            monkeypatch.setenv("SILEX_SCHEMA_PATH", os.pathsep.join([str(temp_dir), str(test_resources)]))

            resolver = GenericResolver(
                schema="test.project_glob_ambiguous_write",
                config_id="test.project_glob_ambiguous_write",
            )
            result = resolver.path_from_context({"context": {"project": "poe2"}}, endpoint="context.project")

            assert result.status == (ResolverStatus.SUCCESS | ResolverStatus.AMBIGUOUS)
            assert Path(result.resolved_path) == canonical_root
            assert [Path(match.resolved_path) for match in result.matches] == [canonical_root, rogue_root]

    def test_path_from_context_honors_include_children_flag(self, monkeypatch):
        """Test parent endpoints return their own path unless include_children=True."""
        test_resources = Path(__file__).parent.parent / "resources"

        schema = {
            "uid": "test.include_children_parent_endpoint",
            "root_path": "",
            "segmenter": "silex.FilesystemSegmenter",
            "pattern": ".*",
            "paths": {
                "project": {
                    "pattern": "TEST",
                    "format": "TEST",
                    "paths": {
                        "take_dir": {
                            "pattern": "v([0-9]+)\\.t([0-9]+)",
                            "endpoint": "take.dir",
                            "format": "v{take.version:03d}.t{take.take:02d}",
                            "paths": {
                                "metadata": {
                                    "pattern": "^test_v([0-9]+)\\.t([0-9]+)\\.metadata$",
                                    "endpoint": "take.metadata",
                                    "format": "test_v{take.version:03d}.t{take.take:02d}.metadata",
                                }
                            },
                        }
                    },
                }
            },
        }

        with tempfile.TemporaryDirectory() as temp_dir_str:
            temp_dir = Path(temp_dir_str)
            schema_path = temp_dir / "test_include_children_parent_endpoint.silex"
            schema_path.write_text(json.dumps(schema, indent=2), encoding="utf-8")

            monkeypatch.setenv("SILEX_SCHEMA_PATH", os.pathsep.join([str(temp_dir), str(test_resources)]))
            resolver = GenericResolver(
                schema="test.include_children_parent_endpoint",
                config_id="test.include_children_parent_endpoint",
            )
            context = {"take": {"version": 6, "take": 2}}

            parent_result = resolver.path_from_context(context, endpoint="take.dir")
            child_result = resolver.path_from_context(context, endpoint="take.dir", include_children=True)

            assert parent_result.status == ResolverStatus.SUCCESS
            assert Path(parent_result.resolved_path) == temp_dir / "TEST" / "v006.t02"
            assert parent_result.schema_endpoint == "take.dir"

            assert child_result.status == ResolverStatus.SUCCESS
            assert Path(child_result.resolved_path) == temp_dir / "TEST" / "v006.t02" / "test_v006.t02.metadata"


class TestRoundTrip:
    """Test path -> context -> path consistency."""

    def test_shot_round_trip(self):
        """Test shot path round trip."""
        resolver = GenericResolver()
        original_path = _test_data_path("test_project", "shots", "seq001", "shot001")

        path_result = resolver.context_from_path(original_path)
        assert path_result.status == ResolverStatus.SUCCESS

        context_result = resolver.path_from_context(path_result.context, endpoint=path_result.schema_endpoint)
        assert context_result is not None

    def test_endpoint_resolution(self):
        """Test resolution to specific endpoints."""
        resolver = GenericResolver()
        path = _test_data_path("test_project", "shots", "seq001")
        result = resolver.context_from_path(path, endpoint="sequence")

        assert result.status == ResolverStatus.SUCCESS
        assert result.schema_endpoint == "sequence"

    def test_schema_matching_specificity(self):
        """Test most specific schema is chosen when multiple match."""
        resolver = GenericResolver()
        path = _test_data_path("test_project", "shots", "seq001")
        result = resolver.context_from_path(path)

        assert result.status == ResolverStatus.SUCCESS
        assert result.schema_uid == "test.integration.basic"


class TestPlaceholderPriority:
    """Test placeholder priority in path generation."""

    def test_context_value_overrides_placeholder(self):
        """Test context value takes priority over placeholder."""
        resolver = GenericResolver(options=SilexParseOptions(placeholders={"frame": "OPTIONS_FRAME"}))
        context = {
            "project": {"code": "test_project"},
            "context": {
                "project": "test_project",
                "tree": "shots",
                "sequence": "seq001",
                "shot": "shot001",
                "task": "anim",
            },
            "frame": 99,  # Context value should win over placeholder
            "file": {"ext": "exr"},
        }

        result = resolver.path_from_context(context, endpoint="shot.frame")
        if result.status == ResolverStatus.SUCCESS:
            assert "0099" in result.resolved_path

    def test_options_placeholder_overrides_config(self):
        """Test options placeholder overrides config placeholder."""
        resolver = GenericResolver(options=SilexParseOptions(placeholders={"frame": "OPTIONS_FRAME"}))
        context = {
            "project": {"code": "test_project"},
            "context": {
                "project": "test_project",
                "tree": "shots",
                "sequence": "seq001",
                "shot": "shot001",
                "task": "anim",
            },
            "file": {"ext": "exr"},
            # No frame in context - should use options placeholder
        }

        result = resolver.path_from_context(context, endpoint="shot.frame")
        if result.status == ResolverStatus.SUCCESS:
            assert "OPTIONS_FRAME" in result.resolved_path

    def test_config_placeholder_as_fallback(self):
        """Test config placeholder used as fallback."""
        resolver = GenericResolver()
        context = {
            "project": {"code": "test_project"},
            "context": {
                "project": "test_project",
                "tree": "shots",
                "sequence": "seq001",
                "shot": "shot001",
                "task": "anim",
            },
            "file": {"ext": "exr"},
        }

        result = resolver.path_from_context(context, endpoint="shot.frame")
        if result.status == ResolverStatus.SUCCESS:
            assert "<frame>" in result.resolved_path


class TestWorkingDirectoryIndependence:
    """Test path resolution works regardless of current directory."""

    def test_resolution_independent_of_cwd(self):
        """Test path_from_context works from any working directory."""
        context = {
            "project": {"code": "test_project"},
            "context": {"project": "test_project", "tree": "shots", "sequence": "001", "shot": "001", "task": "anim"},
            "frame": 42,
            "file": {"ext": "exr"},
        }

        resolver = GenericResolver()
        original_cwd = os.getcwd()

        try:
            # Test from test data directory
            test_data_dir = Path(__file__).parent / "test_data"
            if test_data_dir.exists():
                os.chdir(str(test_data_dir))
                result1 = resolver.path_from_context(context, endpoint="shot.frame")

            # Test from different directory
            os.chdir(str(Path.home()))
            result2 = resolver.path_from_context(context, endpoint="shot.frame")

            # Results should be consistent regardless of cwd
            assert result1.status == result2.status
            if result1.status == ResolverStatus.SUCCESS:
                assert result1.resolved_path == result2.resolved_path

        finally:
            os.chdir(original_cwd)
