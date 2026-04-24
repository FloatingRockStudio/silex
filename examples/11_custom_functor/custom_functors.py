# (C) Copyright 2026 Floating Rock Studio Ltd
# SPDX-License-Identifier: MIT

"""Helpers for wiring custom functor registrations in the executable examples."""

from __future__ import annotations

from silex.core import Registry


def build_lower_slug_functor(registry: Registry):
    """Return the built-in lower functor to back a project-local custom UID."""

    return registry.get_functor("lower")