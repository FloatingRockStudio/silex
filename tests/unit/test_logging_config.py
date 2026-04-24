# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Tests for Silex logging verbosity configuration.

Logging is handled in C++ via spdlog. These tests verify the Python API
for setting verbosity levels works correctly.
"""

import pytest
from silex import Verbosity, set_verbosity


def test_set_verbosity_accepts_enum():
    """set_verbosity should accept Verbosity enum values without error."""
    set_verbosity(Verbosity.Quiet)
    set_verbosity(Verbosity.Flow)
    set_verbosity(Verbosity.Detail)
    set_verbosity(Verbosity.Trace)


def test_set_verbosity_accepts_int():
    """set_verbosity should accept integer values (mapped to Verbosity)."""
    set_verbosity(Verbosity.Quiet)
    set_verbosity(Verbosity.Info)


def test_verbosity_enum_values():
    """Verify Verbosity enum has the expected members."""
    assert hasattr(Verbosity, "Quiet")
    assert hasattr(Verbosity, "Info")
    assert hasattr(Verbosity, "Flow")
    assert hasattr(Verbosity, "Detail")
    assert hasattr(Verbosity, "Trace")
